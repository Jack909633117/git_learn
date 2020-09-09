#power on
echo 16 > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio16/direction
echo 0 > /sys/class/gpio/gpio16/value
sleep 0.1
echo 1 > /sys/class/gpio/gpio16/value
sleep 0.1

#load firmware
/usr/bin/hciattach -s 115200 /dev/ttyS2 texas 115200 flow

#add dbus dir
rm -r /usr/local/bluez/var/run/dbus
ln -s /run/dbus/ /usr/local/bluez/var/run/
rm -rf /usr/local/bluez/var/run/dbus/pid 
mkdir /run/dbus 


#boot bluez process                                                                       
/usr/bin/dbus-daemon --system                                                              
sleep 1                                                                                    
/usr/bin/bluetoothd -C &                                                                   
sleep 1 

#run user app                                                                        
python /usr/local/qbox10/bluetooth/test/simple-agent &
sleep 0.1

if false ; then
#Bluetooth SPP
python /usr/local/qbox10/bluetooth/SPP-loopback.py &
else
#Bluetooth profile
/usr/bin/hciconfig hci0 up
sleep 0.1
/usr/bin/hciconfig hci0 leadv
fi

sleep 0.1
/usr/bin/hciconfig hci0 name "bluez_test"
/usr/bin/hciconfig hci0 piscan
