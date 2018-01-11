#include "ConfigLoad.h"
#include "MLog.h"
#include <iostream>
// #include <unistd.h> //sleep
using namespace MThttpd;
int main(int argc, const char *agrv[])
{
    std::cout << "here" << std::endl;
    _LOG(Level::INFO, {"log1", WHERE});
    _LOG(Level::WARN, {"log2", WHERE});
    _LOG(Level::WARN, {"log3", WHERE});
    _LOG(Level::WARN, {"log4", WHERE});
    errno = EACCES;
    _LOG(Level::ERROR, {"logX", WHERE});
    // _LOG(Level::WARN, {"log5", WHERE});
    // _LOG(Level::WARN, {"log6", WHERE});
    // auto log = MLog::GetIns();
    // log->stop();

    return 0;
}