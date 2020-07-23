
echo 25 > /sys/class/gpio/export
echo 26 > /sys/class/gpio/export
echo in > /sys/class/gpio/gpio25/direction
echo in > /sys/class/gpio/gpio26/direction
keyL=`cat /sys/class/gpio/gpio26/value`
keyR=`cat /sys/class/gpio/gpio25/value`
if [[ $keyL -ne 0 ]] || [[ $keyR -ne 0 ]];then
    exit 0
fi

# -------- prepare --------

# close buzzer
himm 0x114F0060 0x660 > /dev/null
ret=`echo 38 > /sys/class/gpio/unexport`
ret=`echo 38 > /sys/class/gpio/export`
ret=`echo out > /sys/class/gpio/gpio38/direction`
ret=`echo 0 > /sys/class/gpio/gpio38/value`

# start lcd
/root/test/lcd_test.sh &

# start led
/root/test/led_test.sh &

# start wifi1
cd /usr/local/fw6118/wifi
./wifi.sh > /dev/null &

# start wifi2 & 4G
cd /usr/local/fw6118/ap/
echo "mode=AP" > ./mode.conf
./mode.sh > /dev/null &

# start gps test
rm /tmp/gps.log
/root/test/gps_test.bin > /dev/null &

# -------- start test --------

echo -e "\n\n ----- test start ----- \n\n" > /tmp/test.log

# test adc
/root/test/adc_test.sh &
if [ $? -eq 0 ];then
    echo -e "\033[32m ADC TEST OK \033[0m \n" >> /tmp/test.log
else
    echo -e "\033[31m ADC TEST ERROR !! \033[0m \n" >> /tmp/test.log
fi

# test rtc
/root/test/rtc_test.sh &
if [ $? -eq 0 ];then
    echo -e "\033[32m RTC TEST OK \033[0m \n" >> /tmp/test.log
else
    echo -e "\033[31m RTC TEST ERROR !! \033[0m \n" >> /tmp/test.log
fi

# wait wifi & 4g
cc=1
while : ;do
    cc=`expr $cc + 2`
    echo -e " ... Under Testing ... "
    sleep 2
    if [ $cc -gt 35 ];then
        break
    fi
done

# test gps
if ls /tmp | grep "gps.log" > /dev/null ;then
    echo -e "\033[32m GPS TEST OK \033[0m \n" >> /tmp/test.log
else
    echo -e "\033[31m GPS TEST ERROR !! \033[0m \n" >> /tmp/test.log
fi
killall gps_test.bin

# test wlan0
if ipaddr | grep "wlan0" > /dev/null ;then
    echo -e "\033[32m WIFI1 TEST OK \033[0m \n" >> /tmp/test.log
else
    echo -e "\033[31m WIFI1 TEST ERROR !! \033[0m \n" >> /tmp/test.log
fi

# test wlan1
if ipaddr | grep "wlan1" > /dev/null ;then
    echo -e "\033[32m WIFI2 TEST OK \033[0m \n" >> /tmp/test.log
else
    echo -e "\033[31m WIFI2 TEST ERROR !! \033[0m \n" >> /tmp/test.log
fi

# test 4G
if ls /dev | grep "ttyUSB0" > /dev/null ;then
    echo -e "\033[32m 4G TEST OK \033[0m \n" >> /tmp/test.log
else
    echo -e "\033[31m 4G TEST ERROR !! \033[0m \n" >> /tmp/test.log
fi

# -------- test end --------


# final
echo -e "\n ----- test final ----- \n" >> /tmp/test.log

cat /tmp/test.log
cp  /tmp/test.log /mnt/devdisk/test.log

sleep 5

# kill led test
ret=`ps | grep -v grep | grep "led_test.sh" | awk '{print $1}'`
if [ "$ret" != "" ];then
    kill -9 $ret
fi

halt
sleep 10

