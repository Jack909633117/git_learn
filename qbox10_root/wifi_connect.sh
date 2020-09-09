if [ -z $2 ]; then
	echo "Fail:  You must specify one parameter!"
	echo "Example:  wifi_connect SSID PSK_KEY"
	exit 1
fi
sleep 1
/usr/share/wl18xx/sta_connect-ex.sh $1 WPA-PSK $2
#/usr/share/wl18xx/sta_connect-ex.sh NETGEAR WPA-PSK royalmoon369
sleep 1
udhcpc -i wlan0
