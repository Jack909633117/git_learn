#power on
echo 16 > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio16/direction
echo 0 > /sys/class/gpio/gpio16/value
sleep 0.2
echo 1 > /sys/class/gpio/gpio16/value
sleep 0.2

#load firmware
hciattach -s 115200 /dev/ttyS2 texas 115200 flow
sleep 1
hciconfig hci0 up
sleep 0.1
hciconfig hci0 iscan
sleep 0.1

#add dbus dir
rm -rf /usr/local/bluez/var/run/dbus/pid
mkdir /run/dbus

#boot bluez process
dbus-daemon --system
sleep 1
bluetoothd &
sleep 1

#run user app
python /usr/local/qbox10/bluetooth/test/simple-agent &
sleep 0.1
python /usr/local/qbox10/bluetooth/SPP-loopback.py &
sleep 0.1
hciconfig hci0 name "qbox10"
hciconfig hci0 piscan
