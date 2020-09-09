#!/bin/sh

checkServerType(){
    line2=`cat /usr/local/qbox10/config/system.conf | grep 'sys-server_type'`
    case $line2 in
        *"=0"*)
            python2.7 /usr/local/qbox10/py_server/server.py &
            echo "anjian server run .."
            exit
            ;;
    esac
}

line=`cat /usr/local/qbox10/config/system.conf | grep 'sys-product_type_sub'`
case $line in
    *"=1"*)
        checkServerType
        ;;
    *"=3"*)
        checkServerType
        ;;
esac
echo "py anjian server didn't run"