#!/bin/sh

# target driver
target=g_acm_ms
if [ "$1" != "" ];then
    target=$1
fi

# -w wait
rmmod $target -w &

# kill ttyGS0
pidNum=`ps | awk '/ttyGS0/{if($3 != "awk") print $1}'`
if [ $pidNum ] ;then
    echo "-----> kill ttyGS0 $pidNum"
    kill -9 $pidNum
fi
