#!/bin/sh

# wait all ready
sleep 10

psLog=/tmp/py_signalr.log

while : ; do

sleep 5
ps > $psLog

ret=`grep "signalr_client.py" $psLog`
if [ "$ret" == "" ]; then
python2.7 /usr/local/qbox10/py_signalr/signalr_client.py &
fi

ret=`grep "process_intercom" $psLog`
if [ "$ret" == "" ]; then
/usr/local/qbox10/py_signalr/process_intercom &
fi

done