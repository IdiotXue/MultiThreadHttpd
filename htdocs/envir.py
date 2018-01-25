#!/usr/bin/python
# -*-coding:utf-8-*-

# chmod 764 envir.py
# 打印环境变量，若GET带参，则参数被赋给QUERY_STRING环境变量
# localhost:8001/envir.py?name=abc&pw=123
import os

if __name__ == '__main__':
    print "Content-type: text/html"
    print  # 空行，表示响应头部结束
    print "<meta charset=\"utf-8\">"
    print '<title>Environ</title>'
    print "<b>环境变量</b><br>"
    print "<ul>"
    for key in os.environ.keys():
        print "<li><span style='color:green'>%30s </span> : %s </li>" % (key, os.environ[key])
    print "</ul>"
