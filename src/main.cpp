#include "MLog.h"
#include "Server.h"
#include <iostream>
// #include <unistd.h> //sleep
using namespace MThttpd;
int main(int argc, const char *agrv[])
{
    Server server;
    server.start();
    return 0;
}