#include <algorithm> //transform
#include <sys/stat.h>
#include <fstream>
#include <unistd.h>   //pipe，dup2
#include <sys/wait.h> //waitpid
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
        msg = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n<HTML><TITLE> Not Found</TITLE>\r\n<BODY><P> The server could not fulfill\r\nyour request because the resource specified\r\nis unavailable or nonexistent.\r\n</BODY></HTML>\r\n ";
        break;
    case ErrorType::SerError:
        msg = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/html\r\n\r\n<p>Http Server Error</p>\r\n";
        break;
    case ErrorType::MeNotIm:
        msg = "HTTP/1.1 501 Method Not Implemented\r\nContent-Type: text/html\r\n\r\n<HTML><HEAD><TITLE>Method Not Implemented\r\n</TITLE></HEAD>\r\n<BODY><P>HTTP request method not supported.\r\n</BODY></HTML>\r\n";
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
        _ExecCgi(stPath);
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
    std::ifstream ifInput(file); //从strace中看到ifstream无阻塞read，每次8191 Byte
    if (!ifInput.good())
    {
        m_errNum = ErrorType::NotFound;
        RequestError();
        return;
    }
    string stRespon("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
    string line;
    while (getline(ifInput, line))
    {
        stRespon += std::move(line + "\r\n");
        if (stRespon.size() > (1024 * 1024)) //stRespon大于1MB时，先写入
        {
            m_pSock->Append(stRespon);
            stRespon = "";
        }
    }
    if (stRespon.size() > 0)
        m_pSock->Append(stRespon);
    _LOG(Level::INFO, {m_pSock->GetAddr(), "SendFile:" + file});
}
/**
 * 处理执行CGI脚本的请求，以fork-exec的方式执行，低效的CGI做法（参考TinyHttp）
 * ！注意！：这是一种非常容易出错、不够合理的做法，Linux下fork()与多线程的协作性很差。
 * （1）可能死锁：fork后的子进程只存在一个线程（调用fork的线程），其他线程会消失，如果恰好其他
 *      线程占有某个锁且突然死亡，没有机会解锁，那么子进程试图对同一个mutex加锁，就会死锁
 * （2）fork和exec之间只能调用异步信号安全的函数，或者可重入的线程安全的函数，man 7 signal中
 *      指出了哪些是异步信号安全的，除此之外的系统调用或库函数都不应被调用。然而下面的做法还是调
 *      用了putenv（其实现中调用了malloc）设置CGI环境变量
 * TODO：
 * （1）一个合理修改：有另一个专门负责执行CGI的程序（进程池）web server与CGI程序之间用socket（UNIX域
 *      或INET域都行）传递需要设置的环境变量和程序执行的结果
 * （2）一个ugly的修改：每个工作线程复制一份extern char **environ环境变量副本，每次修改环境变量只修改
 *      自身的这份副本，之后调用execle()时就可以将这份副本作为新程序的执行环境
 */
void Request::_ExecCgi(const string &path)
{
    int fdCgiOut[2]; //匿名管道，从子进程写到主进程
    int fdCgiIn[2];  //匿名管道，从主进程写到子进程
    if (pipe(fdCgiOut) < 0)
    {
        m_errNum = ErrorType::SerError;
        _LOG(Level::WARN, {m_pSock->GetAddr(), WHERE, "pipe fail"});
        RequestError();
        return;
    }
    if (pipe(fdCgiIn) < 0)
    {
        m_errNum = ErrorType::SerError;
        _LOG(Level::WARN, {m_pSock->GetAddr(), WHERE, "pipe fail"});
        RequestError();
        return;
    }
    pid_t pid;
    if ((pid = fork()) < 0)
    {
        m_errNum = ErrorType::SerError;
        _LOG(Level::WARN, {m_pSock->GetAddr(), WHERE, "fork() fail"});
        RequestError();
        return;
    }
    int status;
    string stRespon("HTTP/1.1 200 OK\r\n");
    if (pid == 0) //子进程，切记exec之前要执行异步信号安全的函数
    {
        //从主进程读,并写回主进程
        dup2(fdCgiIn[0], STDIN_FILENO);   //把STDIN重定向到fdCgiInput的读取端
        dup2(fdCgiOut[1], STDOUT_FILENO); //把STDOUT重定向到fdCgiOut的写端
        close(fdCgiIn[1]);
        close(fdCgiOut[0]);
        string stMethod("REQUEST_METHOD=");
        char cMethod[32]; //putenv可能直接把字符串指针放入环境链表，不能让变量在exec前就析构，所以专门放在外面
        char cQuery[256];
        char cContLen[32];
        string stRespon = "";
        if (m_method == Method::GET)
        {
            stMethod += "GET";
            copy(stMethod.begin(), stMethod.end(), cMethod);
            string stQuery = "QUERY_STRING=" + m_stQuery;
            copy(stQuery.begin(), stQuery.end(), cQuery);
            cQuery[stQuery.size()] = '\0';
            putenv(cMethod); //ugly，不是信号安全的函数
            putenv(cQuery);
        }
        else
        {
            stMethod += "POST";
            copy(stMethod.begin(), stMethod.end(), cMethod);
            string stContentLen = "CONTENT_LENGTH=" + std::to_string(m_stQuery.size());
            copy(stContentLen.begin(), stContentLen.end(), cContLen);
            cContLen[stContentLen.size()] = '\0';
            putenv(cMethod);
            putenv(cContLen);
        }
        execl(path.c_str(), path.c_str(), (char *)0); //这种执行方式必须把py文件chmod u+x file
        //TODO:这种方式子进程无法执行完退出，导致pipe没关闭，父进程一直阻塞在read，待找出原因
        // execl("/usr/bin/python", path.c_str(), (char *)0);
        close(fdCgiIn[0]); //execl若成功，则不会执行这3句
        close(fdCgiOut[1]);
        exit(EXIT_FAILURE);
    }
    else //父进程
    {
        close(fdCgiIn[0]);
        close(fdCgiOut[1]);
        //post请求由子进程设置CONTENT_LENGTH环境变量，CGI程序根据该长度信息，读取主进程传递的HTTP请求主体
        if (m_method == Method::POST)
        {
            write(fdCgiIn[1], m_stQuery.c_str(), m_stQuery.size()); //阻塞写，写完返回，不用等待另一端读
            printf("post write query %lu\n", m_stQuery.size());
        }
        char buf[4096];                                //pipe buf的大小一般为4096bytes
        int ret = read(fdCgiOut[0], buf, sizeof(buf)); //阻塞读
        //fdCgiOut写端关闭时会返回0,此处相当于等待子进程执行完毕，确保所有的输出都放入stRespon
        while (ret > 0)
        {
            stRespon += string(buf, ret);
            if (stRespon.size() > 1024 * 1024) //待写数据大于1MB
            {
                m_pSock->Append(stRespon);
                stRespon = "";
            }
            ret = read(fdCgiOut[0], buf, sizeof(buf));
        }
        close(fdCgiIn[1]);
        close(fdCgiOut[0]);
        waitpid(pid, &status, 0);
        printf("child process exit %d\n", status); //0:success,1:fail
        if (status == EXIT_FAILURE)
        {
            m_errNum = ErrorType::SerError;
            RequestError();
            _LOG(Level::WARN, {m_pSock->GetAddr(), WHERE, "child execl() fail"});
            return;
        }
        if (stRespon.size() > 0) //子进程不出错才能到这里
            m_pSock->Append(stRespon);
        _LOG(Level::INFO, {m_pSock->GetAddr(), "ExecCgi:" + path});
    }
}