#!/bin/sh
himm 0x112C0090 0x1000
#himm 0x100C007C 0x1001
#himm 0x100C0080 0x1001
net=`lsusb`

timeout2=0


function start_4g ()
{
	echo start_4g
	#on_off
	cd /sys/class/gpio/
	echo 72 > export
	cd gpio72/
	echo out > direction
	echo 1 > value
	sleep 1
	echo 0 > value
	#sleep 1
	#echo 1 > value

	#wake up
	cd /sys/class/gpio/
	echo 30 > export
	cd gpio30/
	echo out > direction
	sleep 1
	echo 0 > value
	sleep 1
	echo 1 > value
	
	#reset
	cd /sys/class/gpio/
	echo 4 > export
	cd gpio4/
	echo out > direction
	sleep 1
	echo 0 > value
	sleep 1
	echo 1 > value
}

if [ -z "$net" ]; 
then	
	echo slave!
else


	echo host 


	start_4g;

	while [ -z "$usb" ]
	do 
		usb=`ls /dev/ttyUSB0`
		#echo 1
		sleep 1
		let "timeout2 += 1"

		if [ $timeout2 -gt 5 ]; then

		        echo "restart 4G ..."
		        timeout2=0
		        start_4g;
		        sleep 1
		    
		fi
	
		
	done

	#sleep 20

	if [ "$1" == "ec20" ]; then
		cd /usr/local/helmet/4g
		echo ec20
		#sleep 5
		#./quectel-CM &
		#./quectel-CM &
		startCmd="/usr/local/helmet/4g/quectel-CM"
	elif [ "$1" == "me909" ]; then
		echo me909
		cd /usr/local/helmet/4g/me909s-ppp
		#sleep 8
		#./huawei-ppp-on
		startCmd="/usr/local/helmet/4g/me909s-ppp/huawei-ppp-on"
	fi

fi


findUsb="true"
timeout=0


# 
while : ; do

sleep 2

# check pppd call
if [ -c "/dev/ttyUSB0" ]; then #&& [ -c "/dev/ttyUSB2" ]

    #timeout2=0
	#echo "check 4g ..."
    # find pppd
    if ps | grep -v grep | grep 'pppd' >> /dev/null ; then
        # network connect
		#echo "pppd exist..."
        if grep 'ppp0' /proc/net/route >> /dev/null ; then
			#echo "ppp0 exist..."			
            timeout=0
        # network disconnect
        else
			#echo "ppp0 not exist..."
            let "timeout += 2"
        fi
    # not found pppd
    else
		#echo "pppd not exist..."
        let "timeout += 2"
    fi

    # timeout, reset usb, restart pppd
    if [ $timeout -gt 10 ]; then

           # echo "restart pppd now ..."
            timeout=0
            $startCmd
            sleep 1
        
    fi
fi

done

