#ifndef SERVER_H
#define SERVER_H
#include <memory>
#include "Socket.h"
#include "ConfigLoad.h"
namespace MThttpd
{
class Server
{
public:
  Server();
  ~Server();

private:
  Socket m_listen; //监听socket
  std::shared_ptr<ConfigLoad> m_conf;
};
}

#endif //SERVER_H