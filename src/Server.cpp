#include "Server.h"
#include "MLog.h"
#include <unistd.h>

using namespace MThttpd;

/**
 * 创建监听套接字，创建并开启工作线程
 */
Server::Server() : m_listen(),
                   m_conf(ConfigLoad::GetIns()),
                   m_tPool(std::stoi(m_conf->GetValue("thread_num")))
{
    // _LOG(Level::INFO, {WHERE, "Server start"});    
    m_listen.Bind(m_conf->GetValue("host"),
                  std::stoi(m_conf->GetValue("port")));
    m_listen.Listen(std::stoi(m_conf->GetValue("listen_num")));
    std::cout << "listen ok\n";
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
    printf("Server: %ld\n", std::this_thread::get_id()); //cout线程不安全
    _LOG(Level::INFO, {WHERE, "Server start"});
    // for (;;)
    for (size_t i = 0; i < 3; ++i)
    {
        // auto sock = m_listen.Accept();
        sleep(1);
    }
}