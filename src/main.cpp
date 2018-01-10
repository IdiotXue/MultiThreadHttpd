#include "ConfigLoad.h"
#include "MLog.h"
#include <iostream>
// #include <unistd.h> //sleep
using namespace MThttpd;
int main(int argc, const char *agrv[])
{
    auto log = MLog::GetIns();
    std::cout << "here" << std::endl;
    log->append(Level::INFO, {"log1", WHERE});
    log->append(Level::WARN, {"log2", WHERE});
    log->append(Level::WARN, {"log3", WHERE});
    log->append(Level::WARN, {"log4", WHERE});
    log->append(Level::WARN, {"log5", WHERE});

    log->stop();
    
    return 0;
}