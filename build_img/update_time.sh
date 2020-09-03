
#update ./rootfs/usr/local/qbox10/config/system.conf -> sys-last_update_time

filePath=./rootfs/usr/local/qbox10/config/system.conf

timeStr=`date +%Y-%m-%d-%H:%M:%S`
line="sys-last_update_time =$timeStr # ($timeStr)"

sed -i "/^sys-last_update_time/c $line" $filePath
