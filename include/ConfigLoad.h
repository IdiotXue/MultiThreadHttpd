#ifndef CONFIGLOAD_H
#define CONFIGLOAD_H

#include <memory>
#include <string>
#include <unordered_map>
#include <iostream> //just for test

namespace MThttpd
{
/**
 * 功能：加载配置文件
 * 定义为singleton类，反正从一开始就得加载，直接用饿汉式
 */
class ConfigLoad
{

public:
  //在类中定义的函数隐式inline
  static const std::shared_ptr<ConfigLoad> GetIns() { return sm_pIns; } //获取实例
  ~ConfigLoad() { std::cout << "ConfigLoad destruct" << std::endl; }
  std::string GetConfValue(const std::string &key) //根据key返回配置参数
  {
    if (m_mapConfig.count(key))
      return m_mapConfig[key];
    return "";
  }
  ConfigLoad(const ConfigLoad &) = delete;
  ConfigLoad &operator=(const ConfigLoad &) = delete;

private:
  ConfigLoad(const std::string &filename); //传入文件名，加载配置
  void trim(std::string &str);             //去空格

private:
  static std::shared_ptr<ConfigLoad> sm_pIns;               //静态唯一实例
  std::unordered_map<std::string, std::string> m_mapConfig; //存放配置
};
}

#endif //CONFIGLOAD_H