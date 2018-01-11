#include "MLog.h"
#include <iostream>
#include <chrono>
#include <ctime> //localtime_r,strftime
#include <cerrno>
#include <cstring> //strerror_r
using namespace MThttpd;
using std::chrono::system_clock;

pthread_once_t MLog::sm_pOnce = PTHREAD_ONCE_INIT; //必须这样初始化
std::shared_ptr<MLog> MLog::sm_pIns;               //定义但没有初始化

MLog::~MLog()
{
    std::cout << "MLog destruct" << std::endl; //just for test
}

/**
 * 打开日志文件
 * @param string
 */
MLog::MLog(const std::string &stLogFile)
    : m_vsCurr(sm_nBufSize), m_vsWrite(sm_nBufSize), m_nIndexC(0), m_nIndexW(0), m_bIsRun(true)
{
    m_fOut.open(stLogFile, std::ofstream::app); //追加模式打开日志文件
    if (!m_fOut.good())
    {
        std::cerr << "open log file fail" << std::endl;
        quick_exit(EXIT_FAILURE); //C++11,多线程用
    }
}

/**
 * 在写日志线程中运行
 */
void MLog::WriteLog()
{
    while (m_bIsRun)
    {
        {
            //必须是unique_lock而不能是lock_guard,因为条件变量等待时要解锁
            std::unique_lock<std::mutex> lck(m_mutex);
            //定时唤醒写日志,while避免伪唤醒。
            //三种情况要唤醒：1.超时。2.待写入缓冲区满。3.程序要结束了。
            //负载极端大时，还未wait待写缓冲区就又满了，则m_WrCond.notify_one()没能通知到m_WrCond.wait_for
            //如果不做m_nIndexC < sm_nBufSize的判断，会使这种情况时MLog性能极差。
            //TODO：再判断一次m_bIsRun是为了降低平稳终止不及时的可能性(依然会)
            if (m_nIndexC < sm_nBufSize && m_bIsRun)
                while (m_WrCond.wait_for(lck, std::chrono::seconds(5)) == std::cv_status::no_timeout)
                    if (!m_bIsRun || m_nIndexC == sm_nBufSize)
                        break;
            if (m_nIndexC) //待写入缓冲有数据
            {
                std::swap(m_nIndexC, m_nIndexW);
                m_nIndexC = 0;                  //待写入缓冲区清空
                std::swap(m_vsCurr, m_vsWrite); //内部实现是move而非复制，所以很快
            }
        }                             //临界区结束
        if (m_nIndexW == sm_nBufSize) //缓冲区满时才有等待在m_EmCond的线程
            m_EmCond.notify_all();
        if (m_nIndexW)
        {
            for (size_t i = 0; i < m_nIndexW; ++i)
                m_fOut << m_vsWrite[i] << '\n'; //不会刷新文件输出缓冲
            m_fOut << std::flush;               //刷新缓冲，磁盘操作写入到文件
            m_nIndexW = 0;                      //写入缓冲区清零
        }
    }
    //这个小bug差点漏掉，有可能写日志线程处于IO操作时，有线程调用了append，同时主线程调用了stop
    //则不会再进入循环体，此时可直接把待写入缓冲区写入日志文件，不用交换了
    std::lock_guard<std::mutex> guard(m_mutex);
    if (m_nIndexC)
    {
        for (size_t i = 0; i < m_nIndexC; ++i)
            m_fOut << m_vsCurr[i] << '\n';
        m_fOut << std::flush; //刷新缓冲，磁盘操作写入到文件
        m_nIndexC = 0;
    }
}
/**
 * 用于初始化唯一实例，只执行一次
 */
void MLog::init()
{
    sm_pIns.reset(new MLog(ConfigLoad::GetIns()->GetValue("logfile")));
    sm_pIns->m_pWrThread = std::make_shared<std::thread>(
        std::bind(&MLog::WriteLog, sm_pIns)); //绑定了唯一单例
}

/**
 * 让其他线程添加日志级别和消息到待写缓冲区
 * @param enum Level
 * @param initializer_list<string>
 */
void MLog::append(Level level, std::initializer_list<std::string> logline)
{
    auto tpNow = system_clock::now();         //time_point
    auto tt = system_clock::to_time_t(tpNow); //time_t,1970/1/1至今的UTC秒数
    struct tm tmBuf;
    localtime_r(&tt, &tmBuf); //将time_t转换为当地时区的时间日期，线程安全，localtime返回static对象的指针多线程不安全
    char formatTime[32];
    strftime(formatTime, sizeof(formatTime), "%Y/%m/%d %X", &tmBuf);
    std::string msg(formatTime);
    switch (level)
    {
    case Level::INFO:
        msg += " INFO ";
        break;
    case Level::WARN:
        msg += " WARN ";
        break;
    case Level::ERROR:
        msg += " ERROR ";
        msg += _GE(errno) + " ";
        break;
    default:
        msg += " UnKown ";
        break;
    }
    for (const auto &str : logline)
        msg += str + " ";
    //此处有3种情况：
    //1.负载较小时：写日志线程超时唤醒，主动交换缓冲区，则循环只执行一次
    //2.负载较大时：写日志线程在缓冲区满时被notify_one唤醒，在m_EmCond释放锁后，m_WrCond获得锁交换缓冲区后notify m_EmCond
    //3.负载极端大时：写日志线程的IO操作未执行完，待写缓冲区又满了，此时可能有多个线程在m_EmCond.wait，notify_one可能通知不到
    //m_WrCond，为使这种情况的性能不下降太多，m_WrCond.wait_for前要做相应判断，见WriteLog函数
    {
        std::unique_lock<std::mutex> lck(m_mutex);
        while (m_nIndexC >= sm_nBufSize)
        {
            m_WrCond.notify_one();                                          //负载极端大时可能通知不到，所以其wait_for前要做判断
            m_EmCond.wait(lck, [this] { return m_nIndexC < sm_nBufSize; }); //进入休眠后才会解锁m_mutex,所以不会收不到通知
        }
        m_vsCurr[m_nIndexC++] = std::move(msg); //msg已无用，移动而不复制
    }
    //发生致命错误时，主动停止日志类，使缓冲区中的消息写入磁盘文件，
    //以便让出错位置接下来抛出异常终止程序
    if (level == Level::ERROR)
        this->stop();
}

/**
 * 平稳终止写日志线程，并等待其将待写入缓冲区的日志记录写入文件
 */
void MLog::stop()
{
    m_bIsRun = false;      //原子类型无需加锁
    m_WrCond.notify_one(); //只有一个写日志线程
    if (m_pWrThread->joinable())
        m_pWrThread->join();
}
/**
 * 解析errno
 * @param int 传入errno
 * @return string errno对应的错误信息
 */
std::string MLog::GetErr(int errnum)
{
    char errBuf[128], *errStr;
    errBuf[0] = '\0';
    //奇怪的线程安全函数，man查了文档，它可能用errBuf也可能直接返回static空间的指针，
    errStr = strerror_r(errnum, errBuf, sizeof(errBuf));
    if (errBuf[0] == '\0')
        return std::string(errStr);
    else
        return std::string(errBuf);
}