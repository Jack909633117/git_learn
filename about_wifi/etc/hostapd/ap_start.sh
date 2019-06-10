#!/bin/sh

ap_name=APM20
ap_passwd=12345678
network_dev=wlan0
network_src=ppp0

driver=`lsmod | grep 'wl18xx'`
if [ -z $driver ]; then
    /root/wl18xx_init.sh
    sleep 2
fi

#
if [ ! -z $1 ]; then
    ap_name=$1
    if [ ! -z $2 ]; then
        ap_passwd=$2
    fi
fi

hostapd_conf_path=/etc/hostapd/hostapd.conf
udhcpd_conf_path=/etc/hostapd/udhcpd.conf

#run ?
if ps | grep -v grep | grep hostapd.conf  > /dev/null
then
    echo "hostapd is already running"
    exit 0
fi

#udhcpd config
if [ ! -f $udhcpd_conf_path ] ; then
   echo "interface $network_dev" > $udhcpd_conf_path
   echo "start 192.168.43.2" >> $udhcpd_conf_path
   echo "end 192.168.43.253" >> $udhcpd_conf_path
   echo "opt dns 8.8.8.8 8.8.4.4" >> $udhcpd_conf_path
   echo "option subnet 255.255.255.0" >> $udhcpd_conf_path
   echo "opt router 192.168.43.1" >> $udhcpd_conf_path
   echo "option lease 864000" >> $udhcpd_conf_path
   echo "option 0x08 01020304" >> $udhcpd_conf_path
fi

#hostapd.conf create
echo "interface=$network_dev" > $hostapd_conf_path
echo "driver=nl80211" >> $hostapd_conf_path
echo "ssid=$ap_name" >> $hostapd_conf_path
echo "hw_mode=g" >> $hostapd_conf_path
echo "channel=10" >> $hostapd_conf_path
echo "macaddr_acl=0" >> $hostapd_conf_path
echo "auth_algs=1" >> $hostapd_conf_path
echo "ignore_broadcast_ssid=0" >> $hostapd_conf_path
if [ -z $ap_passwd ];then
    echo "wpa=0" >> $hostapd_conf_path
else
    echo "wpa=2" >> $hostapd_conf_path
    echo "wpa_passphrase=$ap_passwd" >> $hostapd_conf_path
    echo "wpa_key_mgmt=WPA-PSK" >> $hostapd_conf_path
fi
echo "wpa_pairwise=TKIP" >> $hostapd_conf_path
echo "rsn_pairwise=CCMP" >> $hostapd_conf_path
echo "ctrl_interface=/var/run/hostapd" >> $hostapd_conf_path

#start
echo 1 > /proc/sys/net/ipv4/ip_forward
ifconfig $network_dev 192.168.43.1 netmask 255.255.255.0 up
hostapd $hostapd_conf_path -B
if ps | grep -v grep | grep udhcpd  > /dev/null
then
    echo "udhcpd is already running"
else
    udhcpd $udhcpd_conf_path
fi

#firewall config
iptables -t nat -A POSTROUTING -o $network_src -j MASQUERADE
iptables -A FORWARD -i $network_src -o $network_dev -m state --state RELATED,ESTABLISHED -j ACCEPT
iptables -A FORWARD -i $network_dev -o $network_src -j ACCEPT

