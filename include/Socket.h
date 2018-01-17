#ifndef SOCKET_H
#define SOCKET_H

#include <netinet/in.h> //sockaddr_in,in_addr
#include <arpa/inet.h>  //htons,htonl,inet_ntop
#include <string>
#include <memory>
#include "Buffer.h"

namespace MThttpd
{
/**
 * 封装非阻塞读写接口、socket基本API，RAII封装套接字描述符创建关闭
 * 每个socket预先分配读写缓冲区：
 * 读缓冲：分包，只有字节流内容可作为一个完整的请求时，才会移出m_rdBuf，需要在处理
 * 写缓冲：数据一次写不完时，已写的部分移出，后面的往前补
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

  int Read(); //非阻塞读socket数据
  size_t RdBufSize() const { return m_rdBuf.size(); }
  //通知读缓冲有len字节已被成功处理，注意：只有分离出一个Request才能算成功
  void NotifyRdBuf(size_t len) { m_rdBuf.discard(len); };
  const char *GetRdPtr() const { return m_rdBuf.GetPtr(); }

  bool NeedWr() const { return m_bWrite; }; //返回套接字上是否有数据待写
  size_t Write(const std::string &data)     //外部处理完Request后传入Response
  {
    m_wrBuf.append(data);
    return _Write();
  }

  Socket(const Socket &) = delete;
  const Socket &operator=(const Socket &) = delete;

private:
  size_t _Write();

private:
  int m_fd;                               //socket描述符
  bool m_bWrite;                          //记录是否有数据要写
  struct sockaddr_in m_addr;              //IPv4域下，保存服务器或连接的客户端socket地址
  static const size_t sm_nBufSize = 1024; //初始时给每个Socket分配1KB的读和写缓冲
  char m_cTmpBuf[sm_nBufSize];            //仅用于recv从内核复制数据到用户空间，固定大小1KB
  Buffer<char> m_rdBuf;                   //读缓冲可变大小，由每个Socket自己维护
  Buffer<char> m_wrBuf;                   //写缓冲可变大小，由每个Socket自己维护
};
}

#endif //SOCKET_H