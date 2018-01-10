#ifndef MLOG_H
#define MLOG_H

#include "ConfigLoad.h"
#include <memory>
#include <pthread.h>
#include <string>
#include <fstream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <string>
#include <initializer_list>
#include <atomic>

namespace MThttpd
{
/**
 * Message Log
 * 功能：日志记录
 * 定义为singleton类，延迟加载，等待ConfigLoad获取配置文件名,pthread_once方式确保线程安全
 * 基于双缓冲队列，多线程写队列，单线程读队列并写入日志文件，定时写日志可让锁竞争更小
 * 双缓冲优点：1.append不用等待IO操作 2.合并多条日志，避免每一条日志都唤醒WriteLog，减少IO操作
 */
enum class Level
{
  INFO,
  WARN,
  ERROR
};
class MLog
{
public:
  static const std::shared_ptr<MLog> GetIns() //返回唯一实例
  {
    pthread_once(&sm_pOnce, &MLog::init); //可以确保多线程时init函数只执行一次，确保多线程安全
    return sm_pIns;
  }
  void append(Level level, std::initializer_list<std::string> msg);
  void stop(); //终止写日志线程，写日志线程绑定了一个MLog的智能指针，要依靠智能指针析构MLog，必须先终止写日志线程;
  ~MLog();
  MLog(const MLog &) = delete;
  MLog &operator=(const MLog &) = delete;

private:
  MLog(const std::string &stLogFile);
  static void init(); //pthread_once只执行一次的函数，用于初始化唯一实例
  void WriteLog();    //在写日志线程中运行

private:
  static std::shared_ptr<MLog> sm_pIns; //静态唯一实例
  static pthread_once_t sm_pOnce;       //必须是非本地变量
  std::ofstream m_fOut;                 //日志文件输出流，销毁时自动close文件描述符
  //用于读取缓冲队列并写入日志文件的线程，不能在构造函数中初始化,
  //因为需要绑定sm_pIns(未构造完毕不能传入)，所以必须是heap object
  std::shared_ptr<std::thread> m_pWrThread;
  static const int sm_nBufSize = 20;   //缓冲区大小，以一条记录60个字符算，1024条记录是60KB，测试的时候设小点
  std::vector<std::string> m_vsCurr;  //待写入缓冲，让其他线程写入日志
  std::vector<std::string> m_vsWrite; //写入日志文件的缓冲
  size_t m_nIndexC;                   //记录m_vsCurr有多少条记录
  size_t m_nIndexW;                   //记录m_vsWrite有多少条记录
  std::mutex m_mutex;                 //对待写缓冲区加锁，两个条件变量共用一个mutex
  std::condition_variable m_WrCond;   //条件变量，定时写日志
  std::condition_variable m_EmCond;   //条件变量，等待待写入缓冲区被清空
  std::atomic<bool> m_bIsRun;         //原子类型，控制写日志文件线程是否继续运行
};
#define WHERE std::string(__FILE__) + ":" + std::to_string(__LINE__)
}

#endif //MLOG_H