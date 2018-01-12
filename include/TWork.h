#ifndef TWORK_H
#define TWORK_H
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <unordered_map>
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
    explicit TWork(int epoNum);
    virtual ~TWork(); //有继承，所以声明为虚析构

    void start();                                //由主线程调用开启工作线程
    void stop();                                 //由主线程调用终止工作线程
    void IOLoop();                               //event loop等待socket消息
    void AddTask(std::shared_ptr<Socket> pSock); //主线程添加任务给任务队列
    void GetTask();                              //工作线程获取任务

    TWork(const TWork &) = delete;
    const TWork &operator=(const TWork &) = delete;

  private:
    std::shared_ptr<std::thread> m_pWT;            //工作线程，要在构造外分配，所以需为指针类型
    std::vector<std::shared_ptr<Socket>> m_vTaskQ; //任务队列
    //在连接断开之前保存每个fd对应的socket（唯一保存的地方），移除时析构Socket会自动关闭fd
    std::unordered_map<int, std::shared_ptr<Socket>> m_Fd2Sock;
    std::mutex m_mutex;         //分配和读取任务时，对任务队列加锁
    std::atomic<bool> m_bIsRun; //原子类型，控制工作线程是否继续运行
    int m_eFd;                  //epoll_create返回的FD
};
}

#endif //TWORK_H