

echo 16 > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio16/direction
echo 0 > /sys/class/gpio/gpio16/value
sleep 0.1
echo 1 > /sys/class/gpio/gpio16/value
sleep 0.1


sleep 1

hciattach -s 115200 /dev/ttyS2 texas 115200 flow

sleep 1

hciconfig hci0 up

/usr/local/qbox10/bluetooth/ble/btgatt-server -i hci0 -t public -r -n $1 &

#hciconfig hci0 leadv 
#开始广播
hcitool -i hci0 cmd 0x08 0x000a 01
#广播信息
#hcitool -i hci0 cmd 0x08 0x0008 13 02 01 06 03 02 80 ff 0b 09 71 62 6f 78 31 30 2d 62 6c 65
