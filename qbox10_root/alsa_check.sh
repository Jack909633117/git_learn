#!/bin/sh

#wmix &

psLog=/tmp/alsa_check.log
errLog=/tmp/alsa_check_err.log

while : ; do

sleep 10
ps > $psLog

ret=`grep "wmix" $psLog`
if [ "$ret" == "" ]; then
    time=`date`
    echo "$time : wmix not found !!" >> $errLog
    wmix &
fi

done
