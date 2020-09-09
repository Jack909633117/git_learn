#!/bin/sh

ret=`ip addr | grep 'wlan'`

if [ "$ret" == "" ]; then
    echo -e "\033[1m\033[40;31m WLAN test error !!\033[0m\n"
    exit 1
else
    echo -e "\033[1m\033[40;32m WLAN test ok ..\033[0m\n"
    exit 0
fi
