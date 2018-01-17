#include "TWork.h"
#include "MLog.h"
#include <string>
#include <unistd.h>

using namespace MThttpd;

TWork::TWork(int epoNum) : m_bIsRun(true), m_nMaxEvents(epoNum),
                           m_upEvents(new struct epoll_event[m_nMaxEvents])
{
    m_eventFd = eventfd(0, EFD_NONBLOCK); //直接设置非阻塞，不用fcntl
    m_epFd = epoll_create(m_nMaxEvents);  //epoNum为epoll监听的数目，m_upEvents的大小不应超过这个数目
    if (m_epFd == -1)
    {
        _LOG(Level::ERROR, {WHERE, "epoll_create fail"});
        RUNTIME_ERROR();
    }
    //把m_eventFd加入epoll监听
    m_ev.events = EPOLLIN | EPOLLET; //被write了就可读
    m_ev.data.fd = m_eventFd;
    int ret = epoll_ctl(m_epFd, EPOLL_CTL_ADD, m_eventFd, &m_ev);
    if (ret != 0)
    {
        _LOG(Level::WARN, {WHERE, "epoll add eventfd fail"});
        RUNTIME_ERROR();
    }
    printf("Twork construct:%d,%d\n", m_eventFd, m_epFd);
}

TWork::~TWork()
{
    close(m_eventFd);
    close(m_epFd);
    printf("Twork destruct:%d,%d\n", m_eventFd, m_epFd);
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
    m_bIsRun = false;   //原子类型无需加锁
    uint64_t count = 1; //至少要8字节的变量，每放一个进任务队列就+1
    int ret;
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        ret = write(m_eventFd, &count, sizeof(count));
    }
    //非阻塞eventfd只有在达到最大值时才会返回-1，且EAGAIN，这只有在链接爆炸多
    //才可能出现，所以此处先不考虑这种情况，确保没有其他错误就行
    if (ret != sizeof(count) && errno != EAGAIN)
    {
        _LOG(Level::ERROR, {WHERE, "write eventfd fail"});
        RUNTIME_ERROR();
    }
    if (m_pWT->joinable())
        m_pWT->join();
}
/**
 * event loop等待socket消息和主线程通知用的eventfd
 * TODO：处理EPOLLERR和EPOLLRDHUP
 */
void TWork::IOLoop()
{
    int nFdNum;
    int fd;
    auto iter = m_Fd2Sock.begin();
    printf("Twork in IOLoop:%d,%d\n", m_eventFd, m_epFd);
    while (m_bIsRun)
    {

        // printf("work: %ld\n", m_pWT->get_id()); //cout线程不安全
        //参数依次为：epoll创建的专用fd，回传待处理事件的数组，每次处理的最大事件数，timeout -1为不确定或永久阻塞
        nFdNum = epoll_wait(m_epFd, m_upEvents.get(), m_nMaxEvents, -1);
        if (nFdNum == -1)
            _LOG(Level::WARN, {WHERE, "epoll_wait fail"}); //也许就错一次，先WARN不终止
        printf("Twork %d, epoll wait %d \n", m_eventFd, nFdNum);
        for (int i = 0; i < nFdNum; ++i)
        {
            fd = m_upEvents[i].data.fd;
            iter = m_Fd2Sock.find(fd);
            if (iter == m_Fd2Sock.end() && fd != m_eventFd) //这应该是不可能出现的，此处仅为开发阶段确认
            {
                _LOG(Level::WARN, {WHERE, "fd not in m_Fd2Sock"});
                continue;
            }
            if ((fd == m_eventFd) && (m_upEvents[i].events & EPOLLIN)) //添加任务或即将结束线程
            {
                printf("Twork %d, eventfd EPOLLIN\n", m_eventFd);
                GetTask();
                continue;
            }
            if (m_upEvents[i].events & EPOLLIN)
            {
                printf("fd:%d EPOLLIN\n", fd);
                int nRead = iter->second->Read();
                if (nRead < 0)
                {
                    printf("errno:%d,%s", errno, _GE(errno).c_str());
                    continue;
                }
                if (nRead == 0)
                {
                    printf("close socket msg size %d,Sock count:%lu\n", nRead, iter->second.use_count());
                    //epoll文档：当指向同一file description（可被dup，fcntl，fork复制）的file descriptor被全部close时，会自动在epoll中移除
                    //目前的程序可以确保：此处erase，使智能指针析构指向的socket，即可close唯一的fd，自动移出epoll
                    m_Fd2Sock.erase(fd);
                    continue;
                }
                auto pSock = iter->second;
                std::string msg(pSock->GetRdPtr(), pSock->RdBufSize());
                pSock->NotifyRdBuf(msg.size()); //此处仅仅为Echo测试
                size_t nWrite = pSock->Write(msg);
                printf("Twork %d, write data %lu\n", m_eventFd, nWrite);
            }
            //注意ET且同时监听IN和OUT时，有EPOLLIN（不知是否需要写缓冲不满）就会有EPOLLOUT
            //只要判断是否真有数据可写就行
            if (m_upEvents[i].events & EPOLLOUT)
            {
                if (!iter->second->NeedWr())
                    printf("EPOLLOUT but no data to write\n");
            }
        }
    }
}
/**
 * 主线程accept后，添加任务（Socket）到某个线程的队列，并用eventfd通知对应线程
 */
void TWork::AddTask(std::shared_ptr<Socket> pSock)
{
    uint64_t count = 1; //至少要8字节的变量，每放一个进任务队列就+1
    int ret;
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        m_vTaskQ.push_back(pSock);
        ret = write(m_eventFd, &count, sizeof(count));
    }
    printf("Twork %d, addtask\n", m_eventFd);
    //非阻塞eventfd只有在达到最大值时才会返回-1，且EAGAIN，这只有在链接爆炸多
    //才可能出现，所以此处先不考虑这种情况，确保没有其他错误就行
    if (ret != sizeof(count) && errno != EAGAIN)
    {
        _LOG(Level::ERROR, {WHERE, "write eventfd fail"});
        RUNTIME_ERROR();
    }
}
/**
 * 获取任务，并将fd添加到epoll和m_Fd2Sock
 */
void TWork::GetTask()
{
    uint64_t count;
    int ret;
    std::vector<std::shared_ptr<Socket>> vTmpTaskQ;
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        std::swap(vTmpTaskQ, m_vTaskQ);
        ret = read(m_eventFd, &count, sizeof(count)); //读出所有write的和，eventfd清零
    }
    if (ret != sizeof(count)) //read不应该失败的
    {
        _LOG(Level::ERROR, {WHERE, "read eventfd fail"});
        RUNTIME_ERROR();
    }
    if (count > vTmpTaskQ.size())
        printf("Twork %d, recive stop eventfd\n", m_eventFd);
    printf("Twork %d, eventfd count:%lu,TaskQ size:%lu\n", m_eventFd, count, vTmpTaskQ.size());
    for (const auto &pSock : vTmpTaskQ)
    {
        int sFd = pSock->GetFD();
        if (m_Fd2Sock.find(sFd) != m_Fd2Sock.end())
        { //不应该存在，fd不close，就不可能再遇到，只有erase了某个fd，对应Socket析构
            //fd才会close，此处只是开发阶段确定一下
            _LOG(Level::WARN, {WHERE, "socket existed"});
            continue;
        }
        m_ev.events = EPOLLIN | EPOLLOUT | EPOLLET; //in,out和边沿触发
        m_ev.data.fd = sFd;
        int ret = epoll_ctl(m_epFd, EPOLL_CTL_ADD, sFd, &m_ev);
        if (ret != 0)
        {
            _LOG(Level::WARN, {WHERE, "epoll add fail"});
            continue;
        }
        m_Fd2Sock[sFd] = pSock; //eppll add成功才保留
        printf("Twork %d, add fd:%d\n", m_eventFd, sFd);
    }
}
