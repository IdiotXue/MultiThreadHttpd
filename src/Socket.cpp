#include "Socket.h"
#include "MLog.h"
#include <sys/socket.h> //socket,bind,listen,accept,setsockopt,recv
#include <unistd.h>     //close
#include <cstring>      //memset
#include <fcntl.h>

#include <iostream> //just for test

using namespace MThttpd;
using std::string;

/**
 * 调用socket函数创建TCP套接字
 */
Socket::Socket() : m_bWrite(false), m_rdBuf(sm_nBufSize), m_wrBuf(sm_nBufSize)
{
    //创建未命名服务器socket
    m_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_fd == -1)
    {
        _LOG(Level::ERROR, {WHERE, "socket fail"});
        RUNTIME_ERROR();
    }
    // 使服务器立即重启时，可重用bind时的地址
    int reuse = 1;
    int ret = setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (ret == -1)
    {
        errno = 2;
        _LOG(Level::ERROR, {WHERE, "setsockopt fail"});
        RUNTIME_ERROR();
    }
    std::cout << "sock default construct" << std::endl;
}
/**
 * 用已有的套接字描述符初始化
 * accept中用
 */
Socket::Socket(int fd, const sockaddr_in &cliAddr)
    : m_fd(fd), m_bWrite(false), m_rdBuf(sm_nBufSize), m_wrBuf(sm_nBufSize)
{
    m_addr.sin_family = cliAddr.sin_family;
    m_addr.sin_port = cliAddr.sin_port;
    m_addr.sin_addr.s_addr = cliAddr.sin_addr.s_addr;
    printf("sock construct %d\n", m_fd);
}

/**
 * 析构时会断开连接
 */
Socket::~Socket()
{
    close(m_fd);
    char buf[20]; //存点分十进制IP地址
    inet_ntop(AF_INET, &m_addr.sin_addr, buf, sizeof(buf));
    _LOG(Level::INFO, {"client:" + string(buf), "port:" + std::to_string(ntohs(m_addr.sin_port)), "close"});
    printf("Socket destruct %d\n", m_fd);
}
/**
 * @param host 服务器程序绑定的网卡地址
 * @param port 监听的端口
 */
void Socket::Bind(const std::string &host, uint16_t port)
{
    memset(&m_addr, 0, sizeof(m_addr)); //清零
    m_addr.sin_family = AF_INET;        //IPv4网络协议协议
    m_addr.sin_port = htons(port);      //主机字节序转化为网络字节序
    //struct in_addr 中的s_addr是二进制IPv4地址结构,sin_addr是in_addr结构
    if (host == "Any")
        m_addr.sin_addr.s_addr = htonl(INADDR_ANY); //INADDR_ANY用于多网卡机器，表示任意网卡的IP都能访问httpd
    else
    {
        struct in_addr sIP;                                                         //IPv4地址结构
        int ret = inet_pton(AF_INET, host.c_str(), reinterpret_cast<void *>(&sIP)); //点分十进制 转化为 二进制IP地址
        if (ret == -1)
        {
            _LOG(Level::ERROR, {WHERE, "inet_pton fail"});
            RUNTIME_ERROR();
        }
        else if (ret == 0)
        {
            _LOG(Level::ERROR, {WHERE, "host error"});
            THROW_EXCEPT("host error");
        }
        m_addr.sin_addr.s_addr = sIP.s_addr;
    }
    //绑定socket,即命名
    if (::bind(m_fd, reinterpret_cast<struct sockaddr *>(&m_addr), sizeof(m_addr)))
    {
        _LOG(Level::ERROR, {WHERE, "bind fail"});
        RUNTIME_ERROR();
    }
}

/**
 * @param int 监听队列大小
 */
void Socket::Listen(int size)
{
    if (::listen(m_fd, size))
    {
        _LOG(Level::ERROR, {WHERE, "listen fail"});
        RUNTIME_ERROR();
    }
}

std::shared_ptr<Socket> Socket::Accept()
{
    /* accept获取连接并返回连接套接字，做进一步业务处理 */
    struct sockaddr_in cliAddr;           //保存客户端地址
    socklen_t nAddrLen = sizeof(cliAddr); //值-结果参数，保存客户端地址结构长度
    int nConnFd = ::accept(m_fd, reinterpret_cast<struct sockaddr *>(&cliAddr), &nAddrLen);
    if (nConnFd == -1)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
        {
            _LOG(Level::WARN, {_GE(errno), WHERE, "accept fail"}); //有些请求每收到为WARN
            THROW_EXCEPT("accept fail");                           //希望调用处捕获
        }
        // printf("Server accept %d,%d,%s\n", nConnFd, errno, _GE(errno).c_str());
        return std::shared_ptr<Socket>(); //返回空智能指针
    }
    char buf[20]; //存点分十进制IP地址
    inet_ntop(AF_INET, &cliAddr.sin_addr, buf, sizeof(buf));
    //每个连接的客户端地址和端口都记录到日志
    _LOG(Level::INFO, {"client:" + string(buf), "port:" + std::to_string(ntohs(cliAddr.sin_port)), "connect"});
    return std::make_shared<Socket>(nConnFd, cliAddr);
}

/**
 * 设置m_fd为非阻塞模式，ioctl貌似有隐藏bug，所以用fcntl
 */
bool Socket::SetNonBlock()
{
    int flag = fcntl(m_fd, F_GETFL, 0); //获取当前文件状态标志
    if (flag == -1)
        return false;
    flag |= O_NONBLOCK;
    if (fcntl(m_fd, F_SETFL, flag) == -1)
        return false;
    return true;
}
/**
 * 非阻塞读取socket数据，保存于Socket对象的读缓冲区
 * @return 0为连接关闭，1为已经读完能读的数据，-1为read出错
 */
int Socket::Read()
{
    int ret = 0;
    ret = recv(m_fd, m_cTmpBuf, sizeof(m_cTmpBuf), MSG_DONTWAIT);
    while (ret > 0)
    {
        m_rdBuf.append(m_cTmpBuf, ret);
        ret = recv(m_fd, m_cTmpBuf, sizeof(m_cTmpBuf), MSG_DONTWAIT);
    }
    if (ret == 0) //连接关闭
        return 0;
    else if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
        return 1;
    _LOG(Level::WARN, {WHERE, _GE(errno), "read fail"});
    return -1;
}

/**
 * 非阻塞方式将Socket对象的写缓冲区数据写入socket
 * @return 返回写入了多少字节
 */
size_t Socket::Write()
{
    int ret = 0;
    size_t oriSize = m_wrBuf.size();
    ret = ::write(m_fd, m_wrBuf.GetPtr(), m_wrBuf.size());
    while (ret > 0)
    {
        m_wrBuf.discard(ret); //已写入ret字节，可以去掉
        if (!m_wrBuf.size())  //若已经写完，终止循环
            break;
        ret = ::write(m_fd, m_wrBuf.GetPtr(), m_wrBuf.size());
    }
    if (ret < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
        _LOG(Level::WARN, {WHERE, _GE(errno), "write fail"});
    m_bWrite = !!m_wrBuf.size();
    return oriSize - m_wrBuf.size();
}
