if [ -z $2 ]; then
	echo "Fail:  You must specify one parameter!"
	echo "Example:  wifi_deal.sh ap net_interface"
	echo "          wifi_deal.sh sta ssid psk_key"
	echo "          wifi_deal.sh stop ap or sta"
	exit 1
fi

if [ "$1" = "stop" ]; then
	if [ "$2" = "ap" ]; then
		/usr/share/wl18xx/ap_stop.sh 
	fi
	if [ "$2" = "sta" ]; then
		/usr/share/wl18xx/sta_stop.sh 
	fi
fi

if [ "$1" = "ap" ]; then
	if [ -n $2 ]; then
		echo "Start ap mode."
		/root/wifi_ap.sh  
		/root/wifi_nat.sh $2 &
	fi
fi


if [ "$1" = "sta" ]; then
	/root/wifi_start.sh  #start wifi
	/root/wifi_connect.sh $2 $3 &
fi

