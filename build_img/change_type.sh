if [ -z $1 ]; then
	echo "Fail:  You must specify one parameter!"
	echo "Example:  change_type.sh Qbox10-Pro"
	exit 1
fi

filePath=./rootfs/usr/local/qbox10/config/system.conf

#timeStr=`date +%Y-%m-%d-%H:%M:%S`
#line="sys-last_update_time =$timeStr # ($timeStr)"

#sed -i "/^sys-last_update_time/c $line" $filePath
line="sys-product_name   =$1 # (Qbox10;Qbox10-Pro)"
sed -i "/^sys-product_name/c $line" $filePath
