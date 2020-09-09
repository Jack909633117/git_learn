#!/bin/sh

killall wmix

wmix -d &
sleep 1
wmixMsg /root/test/startrecord.wav -v 10
sleep 1
wmixMsg -r /tmp/xxx.wav -rt 5
sleep 6
wmixMsg /root/test/playrecord.wav -v 10
sleep 1
wmixMsg /tmp/xxx.wav
sleep 6

killall wmix

exit 0
