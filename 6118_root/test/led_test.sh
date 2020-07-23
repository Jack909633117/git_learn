
himm 0x114F0008 0x00000630 > /dev/null
himm 0x114F000C 0x00000630 > /dev/null

ret=`echo 22 > /sys/class/gpio/unexport`
ret=`echo 22 > /sys/class/gpio/export`
ret=`echo out > /sys/class/gpio/gpio22/direction`

ret=`echo 23 > /sys/class/gpio/unexport`
ret=`echo 23 > /sys/class/gpio/export`
ret=`echo out > /sys/class/gpio/gpio23/direction`

while : ;do

echo 1 > /sys/class/gpio/gpio22/value
echo 0 > /sys/class/gpio/gpio23/value

sleep 0.5

echo 0 > /sys/class/gpio/gpio22/value
echo 1 > /sys/class/gpio/gpio23/value

sleep 0.5

done

