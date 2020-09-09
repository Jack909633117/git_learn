#!/bin/sh

#alsa
/usr/local/alsa/sbin/alsactl restore > /dev/null

#wlan
ret=`lsmod | grep 'wl18xx'`
if [ "$ret" == "" ]; then
    /root/wl18xx_init.sh > /dev/null
fi

#bt
ret=`hciconfig hci0 | grep 'hci0:'`
if [ "$ret" == "" ]; then
    #power on
    if [ -e /sys/class/gpio/gpio16 ]; then
        echo 16 > /sys/class/gpio/unexport
    fi
    echo 16 > /sys/class/gpio/export
    echo out > /sys/class/gpio/gpio16/direction
    echo 0 > /sys/class/gpio/gpio16/value
    sleep 0.1
    echo 1 > /sys/class/gpio/gpio16/value
    sleep 0.1
    #load firmware
    /usr/bin/hciattach -s 115200 /dev/ttyS2 texas 115200 flow > /dev/null
    sleep 0.1
    /usr/libexec/bluetooth/bluetoothd -C > /dev/null &    #--compat -Cdn
    sleep 0.1
    /usr/bin/hciconfig hci0 up > /dev/null
    sleep 0.1
    /usr/bin/hciconfig hci0 iscan > /dev/null
    sleep 0.1
    /usr/bin/hciconfig hci0 piscan > /dev/null
fi

