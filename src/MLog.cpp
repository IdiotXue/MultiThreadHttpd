#include "MLog.h"
#include <iostream>
#include <unistd.h> //sleep
using namespace MThttpd;

pthread_once_t MLog::sm_pOnce = PTHREAD_ONCE_INIT; //必须这样初始化
std::shared_ptr<MLog> MLog::sm_pIns;               //定义但没有初始化

MLog::~MLog()
{
    std::cout << "MLog destruct" << std::endl; //just for test
}

/**
 * 打开日志文件
 * @param string
 */
MLog::MLog(const std::string &stLogFile)
{
    m_fOut.open(stLogFile, std::ofstream::app); //追加模式打开日志文件
    if (!m_fOut.good())
    {
        std::cerr << "open log file fail" << std::endl;
        exit(-1);
    }
}

/**
 * 用于初始化唯一实例，只执行一次
 */
void MLog::init()
{
    sm_pIns.reset(new MLog(ConfigLoad::GetIns()->GetConfValue("logfile")));
    sm_pIns->m_pWrThread = std::make_shared<std::thread>(
        std::bind(&MLog::WriteLog, sm_pIns));
    sm_pIns->m_pWrThread->detach(); //不分离就得在合适的地方join
}
/**
 * 在写日志线程中运行
 */
void MLog::WriteLog()
{
    // m_fOut << "succeed in m_tWrite" << std::endl;
    m_fOut << sm_pIns.use_count() << std::endl;
}
