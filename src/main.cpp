#include "ConfigLoad.h"
#include "MLog.h"
#include <iostream>
#include <unistd.h> //sleep
using namespace MThttpd;
void func()
{
    auto conf = ConfigLoad::GetIns();
    std::cout << conf->GetConfValue("port") << std::endl;
    std::cout << conf->GetConfValue("logfile") << std::endl;
    auto log = MLog::GetIns();
}
int main(int argc, const char *agrv[])
{
    func();
    std::cout << "here" << std::endl;
    sleep(1);
    // return 0;
}