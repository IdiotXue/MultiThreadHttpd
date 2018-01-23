#ifndef REQUEST_H
#define REQUEST_H

#include <string>
#include <unordered_map>
#include <memory>
#include "Socket.h"

namespace MThttpd
{
/**
 * 到了这部分只剩下解析字符串和做出响应，简单点都写在Request里，不另外写一个Response
 * 注意：Request类包含了字符串解析和响应两部分工作，名字不准确
 */
class Request
{
  public:
    enum class Method
    {
        GET,
        POST
    };
    explicit Request(std::shared_ptr<Socket> pSock) { m_pSock = pSock; }
    ~Request() {}

    Request(const Request &) = delete;
    const Request &operator=(const Request &) = delete;
    //请求解析
    bool Parse();  //解析请求
    bool GetReq(); //找出读缓冲里的第一个完整请求

    //响应
    void BadRequest();
    void Response();

  private:
    void _Clear();                                         //每次重新获取请求前，都要清空成员变量
    static int _FindFirstReq(const char *str, size_t len); //分包

  private:
    std::shared_ptr<Socket> m_pSock;
    std::string m_stReq;                                      //包含请求内容的字符串
    Method m_method;                                          //请求方法,只保留GET和POST
    std::string m_stUrl;                                      //请求的url
    std::string m_stQuery;                                    //Get请求所带参数
    std::string m_stVersion;                                  //只以字符串形式保留HTTP版本并原样返回
    std::unordered_map<std::string, std::string> m_mapHeader; //保存一系列头部信息
};
}

#endif //REQUEST_H