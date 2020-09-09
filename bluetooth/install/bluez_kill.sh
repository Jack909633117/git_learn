killall hciattach
killall hciconfig hci0 up                                                                             
killall dbus-daemon --system                                                              
killall bluetoothd                                                                     
killall python

echo 0 > /sys/class/gpio/gpio16/value
