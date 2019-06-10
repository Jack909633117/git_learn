#!/bin/sh

network_dev=wlan0

result=`wpa_cli -i$network_dev scan`
case $result in
    *OK*)
        sleep 3
        wpa_cli -i$network_dev scan_results
        sleep 1
        ;;
    *)
        wpa_cli -i$network_dev scan_results
        sleep 1
        ;;
esac
