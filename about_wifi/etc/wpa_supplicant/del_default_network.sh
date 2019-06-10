#!/bin/sh
if [ -z $1 ]; then
    echo "example: $0 ppp0 or wlan0 or eth0 or ..."
    exit
fi
ip route del default via 0.0.0.0 dev $1

