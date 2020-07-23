#!/bin/sh

psLog=/tmp/ps.log

while : ; do

sleep 5
ps > $psLog

ret=`grep "process_log" $psLog`
if [ "$ret" == "" ]; then
cd /usr/local/fw6118
./process_log &
cd -
fi

ret=`grep "process_position" $psLog`
if [ "$ret" == "" ]; then
cd /usr/local/fw6118
./process_position &
cd -
fi

ret=`grep "process_netcheck" $psLog`
if [ "$ret" == "" ]; then
cd /usr/local/fw6118
./process_netcheck &
cd -
fi

ret=`grep "process_rtsp2rtmp" $psLog`
if [ "$ret" == "" ]; then
cd /usr/local/fw6118
./process_rtsp2rtmp "rtsp://192.168.80.1:554/h264minor" &
cd -
fi

ret=`grep "qt5" $psLog`
if [ "$ret" == "" ]; then
cd /usr/local/fw6118
./qt5 rtsp://192.168.80.1:554/h264minor &
cd -
fi

ret=`grep "ptz.pyc" $psLog`
if [ "$ret" == "" ]; then
python /usr/local/fw6118/camera_ptz/ptz.pyc &
fi

ret=`grep "ToolDotCheck.pyc" $psLog`
if [ "$ret" == "" ]; then
python /usr/local/fw6118/dotcheck/ToolDotCheck.pyc &
fi

ret=`grep "server.pyc" $psLog`
if [ "$ret" == "" ]; then
python /usr/local/fw6118/server/server.pyc -m 1 -a &
fi

ret=`grep "process_repair" $psLog`
if [ "$ret" == "" ]; then
cd /usr/local/fw6118
./process_repair &
cd -
fi

done
