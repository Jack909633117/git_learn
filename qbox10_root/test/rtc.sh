#!/bin/sh

hwclock -w > /dev/null

ret1=`hwclock -r`

sleep 1

ret2=`hwclock -r`

if [ "$ret1" == "$ret2" ]; then
    echo -e "\033[1m\033[40;31m RTC test error !!\033[0m\n"
    exit 1
else
    echo -e "\033[1m\033[40;32m RTC test ok ..\033[0m\n"
    exit 0
fi
