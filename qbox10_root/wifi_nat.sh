if [ -z $1 ]; then
	echo "Fail:  You must specify one parameter!"
	echo "Example:  wifi_nat.sh net_interface"
	exit 1
fi
while [ true ]              
do   
	line=$(ifconfig | grep wlan0)
	echo $line                                 
	if [ -n "$line" ]; then
		echo "Start wifi nat service."
		iptables -A FORWARD -s 192.168.43.0/24 -o $1 -j ACCEPT
		iptables -t nat -A POSTROUTING -o $1 -j MASQUERADE
		exit 1
	fi
	sleep 0.5
done
#iptables -t nat -A POSTROUTING -s 192.168.43.0/255.255.255.0 -o ppp0 -j MASQUERADE
#echo 1 >/proc/sys/net/ipv4/ip_forward

