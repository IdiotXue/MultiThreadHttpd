#!/usr/bin/python
# -*-coding:utf-8-*-

# chmod 764 post_x_www_form.py
# 获取POST请求的参数：localhost:8001/post_x_www_form.py
# 注意：post是从HTTP请求报文的主体读取参数的，Content-Length存了主体的长度，这和GET不同

''' 
# CGI处理模块
import cgi
import cgitb

# 创建 FieldStorage 的实例化
# 这个类也是通过os.environ[key]获取环境变量的，然后做字符串处理
form = cgi.FieldStorage()

# 获取数据
query_name = form.getvalue('name')
query_pw = form.getvalue('pw')
'''

# 不依赖cgi模块，自己处理这个过程如下：
# （1）从标准输入（被server重定向到了pipe）读取后，server对应线程从
import os
import sys
from urllib import unquote  # 做url decode

if __name__ == '__main__':

    if 'CONTENT_LENGTH' not in os.environ.keys():
        os._exit(1)  # 0:success,1:fail

    content_len = int(os.environ['CONTENT_LENGTH'])
    query = sys.stdin.read(content_len)  # 从标准输入读取指定长度的字节数
    # 字符串解析
    query_list = query.split('&', query.count('&'))
    param_dict = {}
    for param in query_list:
        key, value = param.split('=', 1)
        key = unquote(key)  # 中文会decode为GBK编码
        value = unquote(value)
        param_dict[key] = value
    query_name = param_dict['name']
    query_pw = param_dict['pw']

    print "Content-type:text/html"
    print
    print "<html>"
    print "<head>"
    print "<meta charset=\"utf-8\">"
    print "<title>Get Query</title>"
    print "</head>"
    print "<body>"
    print "<h2>Name:%s Password:%s</h2>" % (query_name, query_pw)
    print "</body>"
    print "</html>"
