#ifndef MLOG_H
#define MLOG_H

#include <memory>
#include <pthread.h>
#include <string>
#include "ConfigLoad.h"
#include <fstream>
#include <thread>
#include <mutex>

namespace MThttpd
{
/**
 * Message Log
 * 功能：日志记录
 * 定义为singleton类，延迟加载，等待ConfigLoad获取配置文件名,pthread_once方式确保线程安全
 * 基于双缓冲队列，多线程写队列，单线程读队列并写入日志文件
 * 优点：1.双缓冲且读写分离，减少锁竞争 2.合并多条日志，减少IO操作
 * TODO:
 * （1）考虑如何平稳结束：m_pWrThread线程中绑定了一个MLog的智能指针，因此线程不结束MLog就至少有两个智能指针;
 *     实测依靠智能指针在析构时join m_pWrThread的做法无效，主线程可能比m_pWrThread更早结束;
 *     在init()中detach线程，也只能防止Error:terminate called without active exception，主线程依然可
 *     能先结束导致MLog析构没调用，如果不关心平稳结束，这样做就够了,反正程序结束会回收；
 *     是，让Server类通过eventfd告诉MLog的WrThread可以清空缓冲区结束任务了
 */
class MLog
{
public:
  static const std::shared_ptr<MLog> GetIns() //返回唯一实例
  {
    pthread_once(&sm_pOnce, &MLog::init); //可以确保多线程时init函数只执行一次，确保多线程安全
    return sm_pIns;
  }
  ~MLog();
  MLog(const MLog &) = delete;
  MLog &operator=(const MLog &) = delete;

private:
  MLog(const std::string &stLogFile);
  static void init();                               //pthread_once只执行一次的函数，用于初始化唯一实例
  // static void WriteLog(std::shared_ptr<MLog> pLog); //在写日志线程中运行
  void WriteLog(); //在写日志线程中运行

private:
  static std::shared_ptr<MLog> sm_pIns; //静态唯一实例
  static pthread_once_t sm_pOnce;       //必须是非本地变量
  std::ofstream m_fOut;                 //日志文件输出流，销毁时自动close文件描述符
  //用于读取缓冲队列并写入日志文件的线程，不能在构造函数中初始化,
  //因为需要传入this指针，所以必须是heap object
  std::shared_ptr<std::thread> m_pWrThread;
  std::mutex m_mutex;
};
}

#endif //MLOG_H