bluez_status_file=/etc/bluetooth/bluez_status
if [ ! -f $bluez_status_file ]; then
	echo "no find bluez_status and creat it"
	touch $bluez_status_file
fi
echo 0 > /etc/bluetooth/bluez_status
#power on
echo 16 > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio16/direction
echo 0 > /sys/class/gpio/gpio16/value
sleep 0.1
echo 1 > /sys/class/gpio/gpio16/value
sleep 0.1

#load firmware
/usr/local/bluez/bin/hciattach -s 115200 /dev/ttyS2 texas 115200 flow
#sleep 0.1 
#/usr/local/bluez/bin/hciconfig hci0 up                                                                             
#sleep 0.1                                                                                     
#/usr/local/bluez/bin/hciconfig hci0 iscan                                                                          
#sleep 0.1

#add dbus dir
rm -r /usr/local/bluez/var/run/dbus
ln -s /run/dbus/ /usr/local/bluez/var/run/
rm -rf /usr/local/bluez/var/run/dbus/pid 
mkdir /run/dbus 


#boot bluez process                                                                       
/usr/local/bluez/bin/dbus-daemon --system                                                              
sleep 0.1                                                                                    
/usr/local/bluez/bin/bluetoothd -C &    #--compat -Cdn                                                                 
#sleep 0.1 

#run user app                                                                        
#/usr/local/python/bin/python /usr/local/qbox10/bluetooth/test/simple-agent &

#/usr/local/python/bin/python /usr/local/qbox10/bluetooth/SPP-loopback.py &

sleep 0.1 
/usr/local/bluez/bin/hciconfig hci0 up                                                                             
sleep 0.1                                                                                     
/usr/local/bluez/bin/hciconfig hci0 iscan                                                                          
sleep 0.1
/usr/local/bluez/bin/hciconfig hci0 piscan
sleep 0.1
#echo "set bluetooth name "$1
#sleep 3
#/usr/local/bluez/bin/hciconfig hci0 name $1

echo 1 > /etc/bluetooth/bluez_status
