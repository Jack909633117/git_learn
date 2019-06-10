#!/bin/sh

network_dev=wlan0
cmd=/etc/wpa_supplicant/tmp.sh

if [ -z $1 ]; then
    echo -e "\n$0 [wifi_name] # no pass word"
    echo "    or    "
    echo "$0 [wifi_name] [wifi_passwd]"
    echo -e "  example: $0 wifi1 12345678\n"
    exit
fi

#get id
id=`wpa_cli -i$network_dev add_network`
echo -e "wpa_cli -i$network_dev add_network\n$id"
if [ $id == "FAIL" ]; then
    exit
fi

#set ssid psk
echo "wpa_cli -i$network_dev set_network $id ssid" \'\"$1\"\' > $cmd
if [ ! -z $2 ]; then
    echo "wpa_cli -i$network_dev set_network $id key_mgmt WPA-PSK" >> $cmd
    echo "wpa_cli -i$network_dev set_network $id psk" \'\"$2\"\' >> $cmd
else
    echo "wpa_cli -i$network_dev set_network $id key_mgmt NONE" >> $cmd
fi

#enable
echo "wpa_cli -i$network_dev enable_network $id" >> $cmd
echo "wpa_cli -i$network_dev select_network $id" >> $cmd

#save_config
echo "wpa_cli -i$network_dev save_config" >> $cmd

#run cmd
chmod a+x $cmd
$cmd

#udhcpc
if ps | grep -v grep | grep udhcpc > /dev/null
then
    killall udhcpc
    sleep 1
fi
udhcpc -i $network_dev &
