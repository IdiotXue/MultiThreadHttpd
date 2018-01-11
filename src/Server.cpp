#include "Server.h"
#include "MLog.h"

using namespace MThttpd;

Server::Server() : m_listen()
{
    m_conf = ConfigLoad::GetIns();
    m_listen.Bind(m_conf->GetValue("host"),
                  std::stoi(m_conf->GetValue("port")));
    m_listen.Listen(std::stoi(m_conf->GetValue("listen_num")));
}

Server::~Server()
{
    auto log = MLog::GetIns();
    log->append(Level::INFO, {WHERE, "Server Stop"});
    log->stop();
}