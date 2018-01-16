#include "Server.h"
#include "MLog.h"
#include <unistd.h> //getpid
#include <limits>

using namespace MThttpd;

/**
 * 创建监听套接字，创建并开启工作线程
 */
Server::Server() : m_listen(),
                   m_conf(ConfigLoad::GetIns()),
                   m_tPool(std::stoi(m_conf->GetValue("thread_num")))
{
    _LOG(Level::INFO, {WHERE, "----------------------------------"});
    m_listen.Bind(m_conf->GetValue("host"),
                  std::stoi(m_conf->GetValue("port")));
    m_listen.Listen(std::stoi(m_conf->GetValue("listen_num")));
    printf("server listen fd:%d\n", m_listen.GetFD());
    for (auto &tWork : m_tPool)
    {
        tWork = std::make_shared<TWork>(
            std::stoi(m_conf->GetValue("epoll_num")));
        tWork->start();
    }
}

/**
 * 服务器终止时先关闭所有工作线程
 * 再关闭写日志线程，使所有缓冲区的日志记录写入
 */
Server::~Server()
{
    for (const auto &pTWork : m_tPool)
        pTWork->stop();
    auto log = MLog::GetIns();
    log->append(Level::INFO, {WHERE, "Server Stop"});
    log->stop();
}

void Server::start()
{
    _LOG(Level::INFO, {WHERE, "Server start", "pid:" + std::to_string(getpid())});
    // for (;;)
    for (size_t i = 0; i < 3; ++i)
    {
        printf("server: %lu\n", i);
        auto pSock = m_listen.Accept();
        pSock->SetNonBlock();
        m_tPool[ChooseTW()]->AddTask(pSock);
    }
}

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