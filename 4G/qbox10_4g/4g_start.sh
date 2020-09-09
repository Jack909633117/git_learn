#!/bin/sh

startCmd="/usr/sbin/ppp.sh me909s"

# check hardware type
timeout=0
if ps | grep -v grep | grep 'pppd' >> /dev/null ; then
    timeout=9999
fi

while [ $timeout -lt 15 ]; do
    # huawei me909s
    if [ -c "/dev/ttyUSB0" ] && [ -c "/dev/ttyUSB2" ]; then
        sleep 3
        startCmd="/usr/sbin/ppp.sh me909s"
        echo "Check huawei me909s ok"
        $startCmd
        sleep 10
        break
    # zte me3630
    elif [ ! -c "/dev/ttyUSB3" ] && [ -c "/dev/ttyUSB2" ] && [ -c "/dev/ttyUSB1" ]; then
        sleep 3
        startCmd="/usr/sbin/ppp.sh wcdma"
        echo "Check zte me3630 ok"
        $startCmd
        sleep 10
        break
    fi
    sleep 1
    let "timeout += 1"
done

# 
findUsb="true"
timeout=0
timeout2=0

# 
while : ; do

sleep 2

# check pppd call
if [ -c "/dev/ttyUSB0" ] && [ -c "/dev/ttyUSB2" ]; then

    timeout2=0

    # find pppd
    if ps | grep -v grep | grep 'pppd' >> /dev/null ; then
        # network connect
        if grep 'ppp0' /proc/net/route >> /dev/null ; then
            timeout=0
        # network disconnect
        else
            let "timeout += 2"
        fi
    # not found pppd
    else
        let "timeout = 9999"
    fi

    # timeout, reset usb, restart pppd
    if [ $timeout -gt 28 ]; then
        if [ "$findUsb" == "true" ]; then
            # echo "reset usb now ..."
            killall pppd
            findUsb="false"
            sleep 2
            echo 1 > /dev/qbox8_4g
            sleep 2
        else
            # echo "restart pppd now ..."
            findUsb="true"
            timeout=0
            $startCmd
            sleep 1
        fi
    fi

else
    findUsb="false"
    let "timeout2 += 2"
    if [ $timeout2 -gt 8 ]; then
        timeout2=0
        echo 1 > /dev/qbox8_4g
    fi
fi

done
