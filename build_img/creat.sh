#!/bin/sh
#系统文件
#     设备 启动      起点          终点     块数   Id  系统
#./system.img1            2048       22527       10240    b  W95 FAT32
#./system.img2           22528      184319       80896   83  Linux
if [ -z $1 ]; then
	echo "Fail:  You must specify one parameter!"
	exit 1
fi
block_total=$(($1*1024*2))
echo $block_total
rootfs_blocks=$(($block_total-20480-20480))
echo $rootfs_blocks

dd if=/dev/zero of=system.img bs=1M count=$1
fdisk ./system.img <<EOF
n
p

20480
+10M
a
1
n
p

40960

t
1
b
w
EOF
dd if=system.img of=head.img bs=512 count=20480

dd if=system.img of=boot.img bs=512 skip=20480 count=20480
# -F 32
mkfs.vfat -n "BOOT" boot.img <<EOF
y
EOF

dd if=system.img of=rootfs.img bs=512 skip=40960 count=$rootfs_blocks
mkfs.ext4 -L "ROOTFS" rootfs.img <<EOF
y
EOF
