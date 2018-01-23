#include <unistd.h>       //getpid
#include <sys/signalfd.h> //signalfd_siginfo
#include <limits>
#include <signal.h>
#include "Server.h"
#include "MLog.h"
#include "Request.h"

using namespace MThttpd;

/**
 * 创建监听套接字，创建工作线程，创建signalfd
 */
Server::Server() : m_bIsRun(true),
                   m_listen(),
                   m_conf(ConfigLoad::GetIns()),
                   m_tPool(std::stoi(m_conf->GetValue("thread_num"))),
                   m_upEvents(new struct epoll_event[sm_nMaxEvents])
{
    //初始化signal fd，在这之前不应创建MLog，否则写日志线程无法继承此信号屏蔽字
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT); //INT 编号2 中断Ctrl+C可触发
    //Block signals so that they aren't handled according to their default dispositions
    int ret = pthread_sigmask(SIG_BLOCK, &mask, NULL); //之后由该线程创建的线程都会继承这些信号屏蔽字
    if (ret != 0)
        THROW_EXCEPT("pthread_sigmask fail" + _GE(ret));
    m_sigFd = signalfd(-1, &mask, SFD_NONBLOCK);
    if (m_sigFd == -1)
        RUNTIME_ERROR();

    _LOG(Level::INFO, {WHERE, "----------------------------------"});
    _LOG(Level::INFO, {WHERE, "Server start", "pid:" + std::to_string(getpid())});
    //初始化listen socket，bind，listen

    m_listen.Bind(m_conf->GetValue("host"),
                  std::stoi(m_conf->GetValue("port")));
    m_listen.Listen(std::stoi(m_conf->GetValue("listen_num")));
    m_listen.SetNonBlock();
    printf("server listen fd:%d\n", m_listen.GetFD());

    //初始化线程池（工作线程）
    for (auto &tWork : m_tPool)
    {
        tWork = std::make_shared<TWork>(
            std::stoi(m_conf->GetValue("epoll_num")), &Server::Handler);
        tWork->start();
    }

    // 初始化epoll并添加listen socket和signal fd
    m_epFd = epoll_create(sm_nMaxEvents); //epoNum为epoll监听的数目，m_upEvents的大小不应超过这个数目
    if (m_epFd == -1)
    {
        _LOG(Level::ERROR, {WHERE, "epoll_create fail"});
        RUNTIME_ERROR();
    }
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = m_listen.GetFD();
    ret = epoll_ctl(m_epFd, EPOLL_CTL_ADD, m_listen.GetFD(), &ev);
    if (ret != 0)
    {
        _LOG(Level::WARN, {WHERE, "epoll add listen fd fail"});
        RUNTIME_ERROR();
    }
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = m_sigFd;
    ret = epoll_ctl(m_epFd, EPOLL_CTL_ADD, m_sigFd, &ev);
    if (ret != 0)
    {
        _LOG(Level::WARN, {WHERE, "epoll add signalfd fail"});
        RUNTIME_ERROR();
    }
}

/**
 * 服务器终止时关闭所有工作线程
 */
Server::~Server()
{
    close(m_sigFd);
    close(m_epFd);
    for (const auto &pTWork : m_tPool)
        pTWork->stop();
}
/**
 * 启动服务器：进入epoll IO循环
 */
void Server::start()
{
    int nFdNum;
    int fd;
    while (m_bIsRun)
    {
        nFdNum = epoll_wait(m_epFd, m_upEvents.get(), sm_nMaxEvents, -1);
        if (nFdNum == -1)
            _LOG(Level::WARN, {WHERE, "epoll_wait fail"}); //也许就错一次，先WARN不终止
        printf("Server, epoll wait %d \n", nFdNum);
        for (int i = 0; i < nFdNum; ++i)
        {
            fd = m_upEvents[i].data.fd;
            if (fd == m_listen.GetFD() && (m_upEvents[i].events & EPOLLIN)) //监听socket的accept
            {
                auto pSock = m_listen.Accept();
                while (pSock) //当没有连接时pSock为空智能指针
                {
                    pSock->SetNonBlock();
                    m_tPool[ChooseTW()]->AddTask(pSock);
                    pSock = m_listen.Accept();
                }
            }
            else if (fd == m_sigFd && (m_upEvents[i].events & EPOLLIN)) //收到SIGINT信号，平稳终止服务程序
            {
                struct signalfd_siginfo fdsi;
                ssize_t ret = read(m_sigFd, &fdsi, sizeof(struct signalfd_siginfo));
                if (ret != sizeof(struct signalfd_siginfo))
                {
                    _LOG(Level::WARN, {WHERE, "read signalfd fail"});
                    continue;
                }
                if (fdsi.ssi_signo == SIGINT)
                {
                    _LOG(Level::INFO, {WHERE, "read SIGINT"});
                    printf("Server Recv SIGINT\n");
                    m_bIsRun = false;
                }
                else
                    _LOG(Level::WARN, {WHERE, "read unexpected signal\n"});
            }
        }
    }
}
/**
 * 目前的规则：选择维持链接少的工作线程
 */
size_t Server::ChooseTW()
{
    size_t index = 0, nMin = std::numeric_limits<size_t>::max();
    for (size_t i = 0; i < m_tPool.size(); ++i)
        if (m_tPool[i]->GetSockSize() < nMin)
        {
            index = i;
            nMin = m_tPool[i]->GetSockSize();
        }
    return index;
}
/**
 * 传入工作线程中的回调函数，用于处理请求
 * 工作线程无需直到如何处理，一切由Server负责统筹
 * @return 0为连接关闭，1为成功处理，-1为读取数据出错
 */
int Server::Handler(std::shared_ptr<Socket> pSock)
{
    int nRead = pSock->Read();
    if (nRead == 0) //连接关闭
        return 0;
    if (nRead < 0)
    {
        _LOG(Level::WARN, {WHERE, _GE(errno), "read fail"});
        return -1;
    }
    printf("Twork read data %d\n", nRead);
    Request request(pSock);
    while (request.GetReq()) //循环处理每一个，直到没有一个完整的请求为止
    {
        if (request.Parse())
        {
            printf("parse success.\n");
            request.Response();
        }
        else
        {
            printf("parse fail.\n");
            request.BadRequest();
        }
    }

    /* //Echo：直接返回
    std::string msg(pSock->GetRdPtr(), pSock->RdBufSize());
    pSock->NotifyRdBuf(msg.size()); //此处仅仅为Echo测试
    size_t nWrite = pSock->Append(msg);
    printf("Twork write data %lu\n", nWrite); */
    return 1;
}
