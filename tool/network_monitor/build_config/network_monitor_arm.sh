#!/bin/sh
cp business/$1/Makefile-bak business/$1/Makefile
if [ $2 == "build" ]; then
    cd business/$1
    if [ ! -L "hiredis" ];then
        ln -s  ../../common/hiredis/ hiredis
    fi 
    make
    cd -
elif [ $2 == "clean" ]; then
    cd business/$1
    make clean
    cd -
elif [ $2 == "distclean" ]; then
    cd business/$1
    make clean
    cd -
elif [ $2 == "install" ]; then
    cd business/$1
    cp network_monitor* $3
    cd -
fi
