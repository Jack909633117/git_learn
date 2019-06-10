#!/bin/sh

network_dev=wlan0

killall hostapd
ifconfig $network_dev down
