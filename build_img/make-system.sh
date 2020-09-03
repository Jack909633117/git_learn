#!/bin/sh
#80MB系统文件
#     设备 启动      起点          终点     块数   Id  系统
#rootfs.img1            2048       22527       10240    b  W95 FAT32
#rootfs.img2           22528      163839       70656   83  Linux

#./system.img1            2048       22527       10240    b  W95 FAT32
#./system.img2           22528      184319       80896   83  Linux

#if [ "$1" = "boot" ]; then
#mkfs.vfat -n "boot" -F 32 -C boot.tmp 10240
#dd if=boot.tmp of=boot.img bs=1M count=15
#fi
#if [ "$1" = "rootfs" ]; then
#dd if=/dev/zero of=rootfs.tmp bs=1M count=75
#mkfs.ext4 -L "rootfs" -t ext4 rootfs.tmp	#76800 #-n "rootfs" -C rootfs.tmp 76800
#dd if=rootfs.tmp of=rootfs.img bs=1M count=80
#fi
#cat boot.img rootfs.img > myrootfs.img

#update ./rootfs/usr/local/qbox10/config/system.conf -> sys-last_update_time
./update_time.sh

if [ -z $2 ]; then
	echo "Fail:  You must specify one parameter!"
	exit 1
fi
./empty_deal.sh delect
if [ "$1" = "creat" ]; then
	./creat.sh $2
fi
if [ "$1" = "copy" ]; then
	if [ "$3" = "mt" ]; then
		./copy-mt.sh
	elif [ "$3" = "wb" ]; then
		./copy-wb.sh
	else
		./copy.sh
	fi
fi
if [ "$1" = "merge" ]; then
	./merge.sh $2
fi
if [ "$1" = "all" ]; then
	./creat.sh $2
	if [ "$3" = "mt" ]; then
		./copy-mt.sh
	elif [ "$3" = "wb" ]; then
		./copy-wb.sh
	else
		./copy.sh
	fi
	./merge.sh $2
fi

if [ "$1" = "test" ]; then
dd if=/dev/zero of=boot1.img bs=1M count=10
fdisk ./boot1.img <<EOF
n
p



a
1
t
b
w
EOF
mkfs.vfat -n "BOOT" boot1.img <<EOF
y
EOF
mkdir boot_tmp
sudo mount -t vfat boot1.img boot_tmp
sudo cp -rf ./boot/* boot_tmp
sync
sudo umount ./boot_tmp

sudo rm -rf ./boot_tmp
fi

./empty_deal.sh add

./Encryption/Encrypt_For_Ubuntu ./system.img E
sudo rm -rf head.img boot.img rootfs.img
#sudo dd if=boot.img of=/dev/sdi
#sudo dd if=system.img of=/dev/sdi
