#!/bin/sh

ret=`hciconfig hci0 | grep 'hci0:'`

if [ "$ret" == "" ]; then
    echo -e "\033[1m\033[40;31m BT test error !!\033[0m\n"
    exit 1
else
    echo -e "\033[1m\033[40;32m BT test ok ..\033[0m\n"
    exit 0
fi
