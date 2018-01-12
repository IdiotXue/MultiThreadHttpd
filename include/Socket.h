#ifndef SOCKET_H
#define SOCKET_H

#include <netinet/in.h> //sockaddr_in,in_addr
#include <arpa/inet.h>  //htons,htonl,inet_ntop
#include <string>
#include <memory>

namespace MThttpd
{
/**
 * 封装非阻塞读写接口、socket基本API
 * RAII封装套接字描述符创建关闭
 * 仅考虑IPv4
 */
class Socket
{
  public:
    Socket();
    Socket(int fd, const sockaddr_in &cliAddr);
    ~Socket();
    void Bind(const std::string &host, uint16_t port);
    void Listen(int size);
    std::shared_ptr<Socket> Accept();
    bool SetNonBlock();          //设置socket为非阻塞模式
    int GetFD() { return m_fd; } //仅在epoll_ctl等必须的场合使用

    Socket(const Socket &) = delete;
    const Socket &operator=(const Socket &) = delete;

  private:
    int m_fd;                  //socket描述符
    bool m_bWrite;             //记录是否有数据要写
    struct sockaddr_in m_addr; //IPv4域下，保存服务器或连接的客户端socket地址
};
}

#endif //SOCKET_H