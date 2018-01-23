#include <algorithm> //transform
#include <sys/stat.h>
#include <fstream>
#include "Request.h"
#include "MLog.h"

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
    {
        m_errNum = ErrorType::BadReq;
        return false;
    }
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
    {
        m_errNum = ErrorType::MeNotIm;
        return false;
    }
    //解析url
    nBegin = nEnd + 1;
    if ((nEnd = m_stReq.find(' ', nBegin)) == string::npos)
    {
        m_errNum = ErrorType::BadReq;
        return false;
    }
    m_stUrl = std::move(m_stReq.substr(nBegin, nEnd - nBegin));
    //判断是否需执行cgi,
    m_bCgi = _IsCgi();
    //解析HTTP版本
    nBegin = nEnd + 1;
    if ((nEnd = m_stReq.find('\r', nBegin)) == string::npos)
    {
        m_errNum = ErrorType::BadReq;
        return false;
    }
    m_stVersion = std::move(m_stReq.substr(nBegin, nEnd - nBegin));
    //解析首部行
    while (nEnd + 2 != m_stReq.size())
    {
        nBegin = nEnd + 2;
        if ((nEnd = m_stReq.find(':', nBegin)) == string::npos)
        {
            m_errNum = ErrorType::BadReq;
            return false;
        }
        string stKey(std::move(m_stReq.substr(nBegin, nEnd - nBegin)));
        nBegin = nEnd + 2;
        if ((nEnd = m_stReq.find('\r', nBegin)) == string::npos)
        {
            m_errNum = ErrorType::BadReq;
            return false;
        }
        m_mapHeader[std::move(stKey)] = std::move(m_stReq.substr(nBegin, nEnd - nBegin));
    }
    //POST请求必定携带主体
    if (m_method == Method::POST)
    {
        //TODO:字符大小写不确定，最好把所有key都统一用transfrom转换
        auto iter = m_mapHeader.find("Content-Length");
        if (iter == m_mapHeader.end())
        {
            m_errNum = ErrorType::BadReq;
            return false;
        }
        int index = stoi(iter->second);
        printf("index:%d read buf size:%lu\n", index, m_pSock->RdBufSize());
        string query(m_pSock->GetRdPtr(), index);
        m_stQuery = std::move(query);
        m_pSock->NotifyRdBuf(index); //去除这部分字符串
        printf("read buf size:%lu,query:%s\n", m_pSock->RdBufSize(), m_stQuery.c_str());
    }
    // printf("method:%s %d,url:%s,version:%s\n", stMethod.c_str(), m_method, m_stUrl.c_str(), m_stVersion.c_str());
    // for (const auto &x : m_mapHeader)
    //     printf("%s:%s\n", x.first.c_str(), x.second.c_str());
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
    if (index < 0)
    {
        printf("index:%d,Rd size:%lu,RdBuf:%s\n", index, m_pSock->RdBufSize(),
               string(m_pSock->GetRdPtr(), m_pSock->RdBufSize()).c_str());
        return false;
    }
    string req(m_pSock->GetRdPtr(), index + 2); //最后一行包含CRLF
    m_stReq = std::move(req);                   //移动赋值
    printf("index:%d,read buf size:%lu\n", index, m_pSock->RdBufSize());
    printf("----\nRequest:\n%s----\n", m_stReq.c_str());
    m_pSock->NotifyRdBuf(index + 4); //去除这部分字符串
    printf("read buf size:%lu\n", m_pSock->RdBufSize());
    return true;
}

/**
 * 请求没有严格按照HTTP协议
 */
void Request::RequestError()
{
    string msg;
    switch (m_errNum)
    {
    case ErrorType::BadReq:
        msg = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\n\r\n\
        <p> Your browser sent a bad request,such as a POST without a Content-Length.</p>\r\n ";
        break;
    case ErrorType::NotFound:
        msg = "HTTP/1.0 404 Not Found\r\nContent-Type: text/html\r\n\r\n<HTML><TITLE> Not Found</TITLE>\r\n<BODY><P> The server could not fulfill\r\nyour request because the resource specified\r\nis unavailable or nonexistent.\r\n</BODY></HTML>\r\n ";
        break;
    case ErrorType::SerError:
        msg = "HTTP/1.0 500 Internal Server Error\r\nContent-Type: text/html\r\n\r\n<p>Http Server Error</p>\r\n";
        break;
    case ErrorType::MeNotIm:
        msg = "HTTP/1.0 501 Method Not Implemented\r\nContent-Type: text/html\r\n\r\n<HTML><HEAD><TITLE>Method Not Implemented\r\n</TITLE></HEAD>\r\n<BODY><P>HTTP request method not supported.\r\n</BODY></HTML>\r\n";
        break;
    default: //ErrorType::Correct
        return;
    }
    size_t nWrite = m_pSock->Append(msg);
    printf("Twork write data %lu\n", nWrite);
    _LOG(Level::INFO, {m_pSock->GetAddr(), "Request Error", std::to_string(int(m_errNum))});
}
/**
 * 正常响应
 */
void Request::Response()
{
    string stPath = "htdocs" + m_stUrl;
    if (stPath[stPath.size() - 1] == '/') //url以'/'结尾
        stPath += "index.html";
    struct stat st = {0};
    if (stat(stPath.c_str(), &st) == -1)
    {
        m_errNum = ErrorType::NotFound;
        RequestError();
        return;
    }
    //或者url是一个目录
    if ((st.st_mode & S_IFMT) == S_IFDIR)
        stPath += "/index.html";
    if ((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH)) //访问的文件可执行
        m_bCgi = true;
    if (m_bCgi)
        _ExecCgi();
    else
        _SendFile(stPath);
}

/**
 * 每次重新获取请求前，都要做clear操作
 */
void Request::_Clear()
{
    m_bCgi = false;
    m_stReq = "";
    m_stUrl = "";
    m_stQuery = "";
    m_stVersion = "";
    m_mapHeader.clear();
    m_errNum = ErrorType::Correct;
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

/**
 * 判断是否cgi，如果是GET带参数，就把参数分离出来
 */
bool Request::_IsCgi()
{
    if (m_method == Method::POST)
        return true;
    //如果是GET
    size_t nPos = 0;
    if ((nPos = m_stUrl.find('?')) == string::npos)
        return false;
    m_stQuery = std::move(m_stUrl.substr(nPos + 1));
    m_stUrl = std::move(m_stUrl.substr(0, nPos));
    return true;
}
/**
 * 对不带参的GET请求，直接输出文件到浏览器
 */
void Request::_SendFile(const string &file)
{
    std::ifstream ifInput(file);
    if (!ifInput.good())
    {
        m_errNum = ErrorType::NotFound;
        RequestError();
        return;
    }
    string msg("HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n");
    string line;
    while (getline(ifInput, line))
    {
        msg += std::move(line + "\r\n");
        if (msg.size() > (1024 * 1024)) //msg大于1MB时，先写入
        {
            m_pSock->Append(msg);
            msg = "";
        }
    }
    if (msg.size() > 0)
        m_pSock->Append(msg);
    _LOG(Level::INFO, {m_pSock->GetAddr(), "SendFile:" + file});
}
/**
 * 处理执行CGI脚本的请求
 */
void Request::_ExecCgi()
{
}