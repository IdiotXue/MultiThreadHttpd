#include "ConfigLoad.h"
#include <fstream>
#include <iostream>
using namespace MThttpd;
using std::string; //just in source file

//饿汉式定义ConfigLoad对象(main之前)
std::shared_ptr<ConfigLoad> ConfigLoad::sm_pIns(new ConfigLoad("./conf/httpd.conf"));

/**
 * 加载配置文件
 * @param string
 */
ConfigLoad::ConfigLoad(const string &filename)
{
    std::ifstream iConf(filename); //析构时自动关闭
    if (!iConf.good())
    {
        //加载配置文件时，日志类还没初始化（日志文件名就在配置文件里）
        std::cerr << filename << " config file open failed" << std::endl;
        exit(-1);
    }
    string stBuf;
    while (getline(iConf, stBuf))
    {
        if (stBuf.empty())
            continue;
        if (stBuf.find("//") == 0 || stBuf.find("#") == 0) //注释行
            continue;
        size_t pos = stBuf.find("=");
        if (pos == string::npos)
            continue;
        string key = stBuf.substr(0, pos);    //取等号前
        string value = stBuf.substr(pos + 1); //取等号后
        trim(value);
        trim(key);
        m_mapConfig[key] = value;
    }
}

/**
 * 去除空格
 * @param string&
 */
void ConfigLoad::trim(string &str)
{
    if (str.empty())
        return;
    str.erase(0, str.find_first_not_of(" "));
    str.erase(str.find_last_not_of(" ") + 1);
}
