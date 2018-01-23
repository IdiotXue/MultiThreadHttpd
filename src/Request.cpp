#include <algorithm>
#include "Request.h"

using namespace MThttpd;
using std::string;

/**
 * 抽取Socket读缓冲区中的第一个请求并做解析
 * @param bool （1）请求解析错误返回false （2）解析正确返回true
 * TODO:考虑更多的错误情况
 */
bool Request::Parse()
{
    size_t nBegin = 0, nEnd = 0;
    //获取请求方式
    if ((nEnd = m_stReq.find(' ')) == string::npos)
        return false;
    //解析请求方法，暂时只接受GET和POST
    string stMethod(std::move(m_stReq.substr(nBegin, nEnd - nBegin)));
    //这样使用toupper的原因：http://en.cppreference.com/w/cpp/string/byte/toupper
    transform(stMethod.begin(), stMethod.end(), stMethod.begin(),
              [](unsigned char c) { return std::toupper(c); });
    if (stMethod == "GET")
        m_method = Method::GET;
    else if (stMethod == "POST")
        m_method = Method::POST;
    else
        return false;
    //解析url
    nBegin = nEnd + 1;
    if ((nEnd = m_stReq.find(' ', nBegin)) == string::npos)
        return false;
    m_stUrl = std::move(m_stReq.substr(nBegin, nEnd - nBegin));
    //解析HTTP版本
    nBegin = nEnd + 1;
    if ((nEnd = m_stReq.find('\r', nBegin)) == string::npos)
        return false;
    m_stVersion = std::move(m_stReq.substr(nBegin, nEnd - nBegin));
    //解析首部行
    while (nEnd + 2 != m_stReq.size())
    {
        nBegin = nEnd + 2;
        if ((nEnd = m_stReq.find(':', nBegin)) == string::npos)
            return false;
        string stKey(std::move(m_stReq.substr(nBegin, nEnd - nBegin)));
        nBegin = nEnd + 2;
        if ((nEnd = m_stReq.find('\r', nBegin)) == string::npos)
            return false;
        m_mapHeader[std::move(stKey)] = std::move(m_stReq.substr(nBegin, nEnd - nBegin));
    }
    printf("method:%s %d,url:%s,version:%s\n", stMethod.c_str(), m_method, m_stUrl.c_str(), m_stVersion.c_str());
    for (const auto &x : m_mapHeader)
        printf("%s:%s\n", x.first.c_str(), x.second.c_str());
    return true;
}

/**
 * 获取读缓冲区中的第一个请求，copy到m_stReq，并从缓冲区中去除这部分字符串
 * @param bool 若没有一个完整的请求返回false，若有返回true
 */
bool Request::GetReq()
{
    _Clear();
    int index = _FindFirstReq(m_pSock->GetRdPtr(), m_pSock->RdBufSize());
    printf("index:%d\n", index);
    if (index < 0)
    {
        printf("index:%d,Rd size:%lu,RdBuf:%s\n", index, m_pSock->RdBufSize(),
               string(m_pSock->GetRdPtr(), m_pSock->RdBufSize()).c_str());
        return false;
    }
    std::string req(m_pSock->GetRdPtr(), index + 2); //最后一行包含CRLF
    m_stReq = std::move(req);                        //移动赋值
    printf("read buf size:%lu\n", m_pSock->RdBufSize());
    printf("----\nRequest:\n%s----\n", m_stReq.c_str());
    m_pSock->NotifyRdBuf(index + 4); //去除这部分字符串
    printf("read buf size:%lu\n", m_pSock->RdBufSize());
    return true;
}

/**
 * 请求没有严格按照HTTP协议
 */
void Request::BadRequest()
{
    string msg = string("HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\nBad Request\r\n\r\n");
    size_t nWrite = m_pSock->Append(msg);
    printf("Twork write data %lu\n", nWrite);
}
/**
 * 正常响应
 */
void Request::Response()
{
    string msg = string("HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\nHello World\r\n\r\n");
    size_t nWrite = m_pSock->Append(msg);
    printf("Twork write data %lu\n", nWrite);
}

/**
 * 每次重新获取请求前，都要做clear操作
 */
void Request::_Clear()
{
    m_stReq = "";
    m_stUrl = "";
    m_stQuery = "";
    m_stVersion = "";
    m_mapHeader.clear();
}
/**
 * 找出读缓冲里的第一个请求，以“\r\n\r\n”可以在字节流中区分两个请求
 * @param int 若找不到返回-1，找到了返回第一个’\r'的下标
 */
int Request::_FindFirstReq(const char *str, size_t len)
{
    if (len < 4)
        return -1;
    len -= 3;
    for (size_t i = 0; i < len; ++i)
        if (str[i] == '\r' && str[i + 1] == '\n' && str[i + 2] == '\r' && str[i + 3] == '\n')
            return i;
    return -1;
}