# Multi-thread Httpd by C++11
- ./build.sh 可以直接编译运行本程序，缺少相应目录时报错
- ConfigLoad: 加载配置文件
- MLog: 日志类
- TWork: 工作线程
- Server: 服务器总开关
- Socket: 封装套接字API
- Buffer: 封装读写缓冲
- Request: 解析和处理请求
- Daemonize:使程序以守护进程方式运行，[其实现参见此链接](https://github.com/IdiotXue/Daemon)
- 注：
    - （1）htdocs目录中的内容参考Tinyhttpd 
    - （2）代码中所有printf和cout皆为方便观察程序运行过程，均可注释掉
    - （3）为便于在终端观察输出，Daemonize未加入
    - （4）httpd.conf为配置文件，httpd.log为日志文件，htddos目录存放所有CGI脚本和HTML页面
