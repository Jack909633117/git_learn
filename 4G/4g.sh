#!/bin/sh
himm 0x112C0090 0x1000
#himm 0x100C007C 0x1001
#himm 0x100C0080 0x1001
net=`lsusb`

if [ -z "$net" ]; 
then	
	echo slave!
else


	echo host 
	cd /sys/class/gpio/
	echo 30 > export
	cd gpio30/
	echo out > direction
	sleep 1
	echo 1 > value

	cd /sys/class/gpio/
	echo 4 > export
	cd gpio4/
	echo out > direction
	sleep 1
	echo 1 > value

	cd /sys/class/gpio/
	echo 72 > export
	cd gpio72/
	echo out > direction
	echo 1 > value
	sleep 1
	echo 0 > value
	sleep 1
	echo 1 > value

	while [ -z "$usb" ]
	do 
		usb=`ls /dev/ttyUSB0`
		#echo 1
		sleep 1
	done
	sleep 1

	#sleep 20

	cd /usr/local/helmet/4g
	#sleep 5
	#./quectel-CM &
	./quectel-CM &

	#while [[ -z "$route" || -z "$route1" ]]
	while [ -z "$route" ]
	do 
		route=`route -n | grep "eth1"`
#		route1=`route -n | grep "usb0"`
#		echo 1
		sleep 0.5
	done	

fi

