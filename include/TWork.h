#ifndef TWORK_H
#define TWORK_H
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <functional>
#include "Socket.h"

namespace MThttpd
{
/**
 * 工作线程work thread，每个TWork对象工作于一个独立的线程拥有自己的任务队列，
 * 用于接收Server主线程分发的Socket；TWork一旦启动就工作于event loop，即循
 * 环于epoll_wait等待事件发生并处理；
 * 
 * 工作线程运行IOLoop函数，其需绑定TWork对象this，为了让成员函数获取自己this的智能指针，
 * 需继承enable_shared_from_this<TWork>，这种方式有两个注意点：
 * （1）必须在TWork已构造后才能调用shared_from_this()，因此需要start函数
 * （2）TWork必须是heap object而不能是stack object，因此server中使用shared_ptr保存TWork
 *     否则会抛出异常throwing an instance of 'std::bad_weak_ptr'
 * 
 * 与MLog相同：m_pWT构造时绑定了TWork对象的智能指针，线程不结束，TWork对象就无法析构，
 * 因此谁创建并start了TWork对象，谁就要在最后stop该对象
 */
class TWork : public std::enable_shared_from_this<TWork>
{
  public:
    typedef std::function<int(std::shared_ptr<Socket>)> TaskHandler;

    TWork(int epoNum, TaskHandler handler);
    virtual ~TWork(); //有继承，所以声明为虚析构

    void start();                                           //由主线程调用开启工作线程
    void stop();                                            //由主线程调用终止工作线程
    void IOLoop();                                          //event loop等待socket消息和主线程通知用的eventfd
    void AddTask(std::shared_ptr<Socket> pSock);            //主线程添加任务给任务队列
    void GetTask();                                         //工作线程获取任务
    // size_t GetSockSize() const { return m_Fd2Sock.size(); } //TODO：没加锁错误，可以用一个atomic<int>来记录连接数

    TWork(const TWork &) = delete;
    const TWork &operator=(const TWork &) = delete;

  private:
    std::shared_ptr<std::thread> m_pWT;            //工作线程，要在构造外分配，所以需为指针类型
    std::vector<std::shared_ptr<Socket>> m_vTaskQ; //任务队列
    //在连接断开之前保存每个fd对应的socket（唯一保存的地方），移除时析构Socket会自动关闭fd
    std::unordered_map<int, std::shared_ptr<Socket>> m_Fd2Sock;
    std::mutex m_mutex;         //分配和读取任务时，对任务队列加锁
    std::atomic<bool> m_bIsRun; //原子类型，控制工作线程是否继续运行
    int m_eventFd;              //线程间通信用，用于主线程添加任务后通知对应工作线程，以类似计数器的方式工作

    //epoll相关参数
    int m_nMaxEvents; //最大监听数目
    //epoll_wait时使用的数组，unique_ptr对动态数组提供了支持：
    //（1）可以下标操作（2）能自动调用delete[]；
    //相比之下，shared_ptr在这方面就弱了（没有提供这两个支持）
    std::unique_ptr<struct epoll_event[]> m_upEvents;
    int m_epFd;              //epoll_create返回的FD
    struct epoll_event m_ev; //GetTask函数中用于添加Fd到epoll

    //TODO:应该有更合理或者可变的实现方式，比如包含一个Request成员
    TaskHandler m_handler; //处理请求的回调函数
};
}

#endif //TWORK_H