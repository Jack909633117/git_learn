ifconfig eth0 up;
sleep 0.8;
ifconfig eth0 172.16.25.198;
route add -net 172.16.0.0 netmask 255.255.0.0 gw 172.16.25.254;
route add -net 0.0.0.0 netmask 0.0.0.0 gw 172.16.25.254;
mount -t nfs -o nolock 172.16.25.85:/mysdb/fw61178/mynfs /nfs
route add -net 172.16.0.0 netmask 255.255.0.0 gw 192.168.3.1;
route add -net 0.0.0.0 netmask 0.0.0.0 gw 192.168.3.1;
#
route add -net 172.16.0.0 netmask 255.255.0.0 gw 192.168.43.1;
route add -net 0.0.0.0 netmask 0.0.0.0 gw 192.168.43.1;
