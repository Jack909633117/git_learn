#!/bin/sh

checkServerType(){
    line2=`cat /usr/local/qbox10/config/system.conf | grep 'sys-server_type'`
    case $line2 in
        *"=0"*)
            python2.7 /usr/local/qbox10/py_websocket/websocket_client.py &
            echo "websocket_client run .."
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
echo "websocket_client didn't run"
