#ifndef SERVER_H
#define SERVER_H
#include <memory>
#include <vector>
#include "Socket.h"
#include "ConfigLoad.h"
#include "TWork.h"
namespace MThttpd
{
/**
 * RAII封装创建启动和结束线程池
 */
class Server
{
  public:
    Server();
    ~Server();
    void start();

  private:
    Socket m_listen; //监听socket
    std::shared_ptr<ConfigLoad> m_conf;
    std::vector<std::shared_ptr<TWork>> m_tPool; //线程池,TWork必须是heap object，其头文件中有解释
};
}

#endif //SERVER_H