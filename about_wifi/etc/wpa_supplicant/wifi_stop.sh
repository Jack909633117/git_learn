#!/bin/sh

network_dev=wlan0

killall wpa_supplicant
killall udhcpc
ip link set $network_dev down

