#!/usr/bin/python
# -*-coding:utf-8-*-

# chmod 764 hello.py
# 这种可以直接改成html文件发送，无需执行CGI，此处只为测试
import os

if __name__ == '__main__':
    print "Content-type:text/html"
    print                               # 空行，表示响应头部结束
    print '<html>'
    print '<head>'
    print '<meta charset="utf-8">'
    print '<title>Hello</title>'
    print '</head>'
    print '<body>'
    print '<h2>Hello  World - 我的第一个 CGI 程序</h2>'
    print '</body>'
    print '</html>'
