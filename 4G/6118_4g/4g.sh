#!/bin/sh
himm 0x112F0034 0x400
cd /sys/class/gpio
echo 6 > export
cd gpio6
echo out > direction
sleep 1
echo 0 > value
sleep 1
echo 1 > value
sleep 1
echo 0 > value

sleep 20

while [ ! -e /dev/ttyUSB1 ]
do
	sleep 1
	# echo "wait..."
done


net=`ifconfig -a | grep "eth1"`

if [ -z $net ]; 
then	
	net=usb0;
	echo "net=$net"
else
	net=eth1;
	echo "net=$net"	
fi

if [ -e /dev/ttyUSB1 ]; then

#	echo -en "at+qnetdevctl=1,1\r\n" > /dev/ttyUSB1
#	ifconfig $net up
#	udhcpc -i $net
	cd /usr/local/fw6118/ap/
	./quectel-pppd.sh /dev/ttyUSB1
fi


startCmd="/usr/local/fw6118/ap/quectel-pppd.sh /dev/ttyUSB1"

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



