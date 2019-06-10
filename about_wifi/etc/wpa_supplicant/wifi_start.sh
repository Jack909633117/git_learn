#!/bin/sh

network_dev=wlan0
wpa_supplicant_conf_path=/etc/wpa_supplicant/wpa_supplicant.conf

driver=`lsmod | grep 'wl18xx'`
if [ -z $driver ]; then
    /root/wl18xx_init.sh
    sleep 2
fi

if ps | grep -v grep | grep wpa_supplicant > /dev/null
then
    echo "wpa_supplicant is running"
else
   if [ ! -f $wpa_supplicant_conf_path ] ; then
       echo "ctrl_interface=/var/run/wpa_supplicant" > $wpa_supplicant_conf_path
       echo "update_config=1" >> $wpa_supplicant_conf_path
   fi
    wpa_supplicant -i$network_dev -Dnl80211 -c$wpa_supplicant_conf_path -B
fi

#udhcpc
if ps | grep -v grep | grep udhcpc > /dev/null
then
    killall udhcpc
    sleep 1
fi
udhcpc -i $network_dev &
