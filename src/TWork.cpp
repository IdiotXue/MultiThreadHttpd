#include "TWork.h"
#include "MLog.h"
#include <sys/epoll.h>
#include <string>
#include <unistd.h>

using namespace MThttpd;

TWork::TWork(int epoNum) : m_bIsRun(true)
{
    m_eFd = epoll_create(epoNum);
    std::cout << "Twork construct:" << m_eFd << std::endl;
}

TWork::~TWork()
{
    close(m_eFd);
    std::cout << "Twork destruct:" << m_eFd << std::endl;
}

/**
 * 开启工作线程，进入event loop
 */
void TWork::start()
{
    m_pWT = std::make_shared<std::thread>(
        std::bind(&TWork::IOLoop, shared_from_this()));
}
/**
 * 通知线程终止后等待其停止
 */
void TWork::stop()
{
    m_bIsRun = false; //原子类型无需加锁
    //eventfd
    if (m_pWT->joinable())
        m_pWT->join();
}
/**
 * event loop等待socket消息
 */
void TWork::IOLoop()
{
    while (m_bIsRun)
    {
        //epoll添加
        printf("work: %ld\n", m_pWT->get_id()); //cout线程不安全
        sleep(1);
    }
}
/**
 * 主线程accept后，添加任务（Socket）到某个线程的队列
 */
void TWork::AddTask(std::shared_ptr<Socket> pSock)
{
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        m_vTaskQ.push_back(pSock);
    }
    //eventfd
}
/**
 * 获取任务
 */
void TWork::GetTask()
{
    std::vector<std::shared_ptr<Socket>> vTmpTaskQ;
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        std::swap(vTmpTaskQ, m_vTaskQ);
    }
    // struct epoll_event ev;
    for (const auto &pSock : vTmpTaskQ)
    {
        int sFd = pSock->GetFD();
        if (m_Fd2Sock.find(sFd) != m_Fd2Sock.end())
        { //不应该存在，fd不close，就不可能再遇到，只有erase了某个fd，对应Socket析构
            //fd才会close，此处只是开发阶段确定一下
            _LOG(Level::WARN, {WHERE, "socket existed"});
            continue;
        }
        m_Fd2Sock[sFd] = pSock;
    }
}
