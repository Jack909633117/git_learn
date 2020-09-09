#!/bin/sh

checkServerType(){
    line2=`cat /usr/local/qbox10/config/system.conf | grep 'sys-server_type'`
    case $line2 in
        *"=1"*)
            python2.7 /usr/local/qbox10/py_signalr/signalr_client.py &
            /usr/local/qbox10/py_signalr/process_intercom &
            /root/py_signalr_check.sh &
            echo "signalr_client &  process_intercom run .."
            exit
            ;;
    esac
}

checkServerType
echo "signalr_client &  process_intercom didn't run"
