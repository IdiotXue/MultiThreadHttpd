#include "MLog.h"
#include "Server.h"
#include <iostream>
// #include <unistd.h> //sleep
using namespace MThttpd;
int main(int argc, const char *agrv[])
{
    std::cout << "here" << std::endl;
    _LOG(Level::INFO, {WHERE, "log1"});
    _LOG(Level::WARN, {WHERE, "log2"});
    Server server;
    return 0;
}