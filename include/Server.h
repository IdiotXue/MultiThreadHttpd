#ifndef SERVER_H
#define SERVER_H
#include <memory>
#include <vector>
#include <sys/epoll.h>
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
    size_t ChooseTW(); //选择工作线程oo

  private:
    int m_sigFd; //signalfd，以file destriptor方式操作信号
    bool m_bIsRun;

    Socket m_listen; //监听socket
    std::shared_ptr<ConfigLoad> m_conf;
    std::vector<std::shared_ptr<TWork>> m_tPool; //线程池,TWork必须是heap object，其头文件中有解释

    static const int sm_nMaxEvents = 2; //目前只需要监听listen socket和signalfd两个FD
    std::unique_ptr<struct epoll_event[]> m_upEvents;
    int m_epFd; //epoll_create返回的FD
};
}

#endif //SERVER_H