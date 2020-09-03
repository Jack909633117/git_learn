#!/bin/sh
line=$(cat /etc/init.d/mmc/first_start) 

anotherRootfs="/mnt/system_bak"

if [ $line -eq 0 ]; then
	echo "1" > /etc/init.d/mmc/first_start
	echo "1" > $anotherRootfs/etc/init.d/mmc/first_start

	echo "[ Qbox: mkfs disk ]"
	/etc/init.d/mmc/mmc_fdisk.sh
	
	sync
	echo "[ Qbox: reboot 1th ]"
	reboot -f
	sleep 30
elif [ $line -eq 1 ]; then
	echo "2" > /etc/init.d/mmc/first_start
	echo "2" > $anotherRootfs/etc/init.d/mmc/first_start

	#mkfs
	echo "[ Qbox: mkfs ]"
	/etc/init.d/mmc/mmc_mkfs.sh

	#move boot regfile & config
	echo "[ Qbox: move boot regfile & config to /root/ ]"
	mv /mnt/boot/regfile /root/
	mv /mnt/boot/config /root/

	sync
	echo "[ Qbox: reboot 2th ]"
	reboot -f
	sleep 30
else
	/etc/init.d/mmc/mmc_p6Check.sh
	/etc/init.d/mmc/mmc_staticCheck.sh

	# config reset
	if [ ! -f "/etc/init.d/mmc/dev_reset" ];then
		echo '0' > /etc/init.d/mmc/dev_reset
		echo "[ Qbox: backup config ]"
		cp /root/config  /usr/local/qbox10/config-bak
		rm /usr/local/qbox10/data_bak -rf
		cp /usr/local/qbox10/data    /usr/local/qbox10/data_bak -rf
		rm /usr/local/qbox10/config_bak -rf
		cp /usr/local/qbox10/config  /usr/local/qbox10/config_bak -rf
	else
		reset_temp=$(cat /etc/init.d/mmc/dev_reset)
		if [ $reset_temp -eq 1 ]; then
			echo '0' > /etc/init.d/mmc/dev_reset
			echo "[ Qbox: reset config ]"
			cp /usr/local/qbox10/config-bak   /root/config
			if [ -d "/usr/local/qbox10/data_bak" ] ; then
				rm /usr/local/qbox10/data -rf
				cp /usr/local/qbox10/data_bak    /usr/local/qbox10/data -rf
			fi
			if [ -d "/usr/local/qbox10/config_bak" ] ; then
				rm /usr/local/qbox10/config -rf
				cp /usr/local/qbox10/config_bak  /usr/local/qbox10/config -rf
			fi
			# mkfs
			/etc/init.d/mmc/mmc_mkfs.sh
			echo "[ Qbox: reboot 3th config reset ]"
			reboot -f
			sleep 30
		fi
	fi

	sync
fi
