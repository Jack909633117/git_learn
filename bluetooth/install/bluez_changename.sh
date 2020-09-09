echo "set bluetooth name "$1
sleep 0.1
/usr/local/python/bin/python /usr/local/qbox10/bluetooth/test/simple-agent &
/usr/local/python/bin/python /usr/local/qbox10/bluetooth/SPP-loopback.py &
sleep 3
/usr/local/bluez/bin/hciconfig hci0 name $1

