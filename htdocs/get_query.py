#!/usr/bin/python
# -*-coding:utf-8-*-

# chmod 764 get_query.py
# 获取GET请求的参数：localhost:8001/get_query.py?name=abc&pw=123
# GET请求的参数存在环境变量QUERY_STRING中

# CGI处理模块
import cgi
import cgitb

if __name__ == '__main__':
    # 创建 FieldStorage 的实例化
    # 这个类也是通过os.environ[key]获取环境变量的，然后做字符串处理
    form = cgi.FieldStorage()

    # 获取数据
    query_name = form.getvalue('name')
    query_pw = form.getvalue('pw')

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
