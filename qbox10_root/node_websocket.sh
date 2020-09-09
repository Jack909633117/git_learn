#!/bin/sh
line=`cat /usr/local/qbox10/config/system.conf | grep 'sys-product_type_sub'`
case $line in
    *"=1"*)
        /usr/bin/node-v4.2.6 /usr/local/qbox10/websocket/websocket_client.js &
        echo "hit type=1 node_websocket run .."
        exit
        ;;
    *"=3"*)
        /usr/bin/node-v4.2.6 /usr/local/qbox10/websocket/websocket_client.js &
        echo "hit type=3 node_websocket run .."
        exit
        ;;
esac
echo "node_websocket didn't run"
