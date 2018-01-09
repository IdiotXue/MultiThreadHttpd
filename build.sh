#!/usr/bin/env bash

# $0 shell本身的文件名
# $? 最后运行的命令的结束代码（返回值）

SCRIPT=$(readlink -f "$0") # 读取本文件的canonical file name
BASEDIR=$(dirname "$SCRIPT") # 获取项目根目录
cd $BASEDIR

if [ ! -d src ] # 判断是否有src这个目录
then
    echo "ERROR: $BASEDIR is missing the src/ directory"
    exit -1
fi

if [ ! -d include ] # 判断是否有include这个目录
then
    echo "ERROR: $BASEDIR is missing the include/ directory"
    exit -1
fi

if [ ! -d obj ] # 判断是否有obj这个目录
then
    echo "ERROR: $BASEDIR is missing the obj/ directory"
    exit -1
fi

if [ ! -d bin ] # 判断是否有bin这个目录
then
    echo "ERROR: $BASEDIR is missing the bin/ directory"
    exit -1
fi

make
./bin/myhttpd
