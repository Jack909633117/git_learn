#!/bin/sh
if [ -z $1 ]; then
	echo "Fail:  You must specify one parameter!"
	exit 1
fi
block_total=$(($1*1024*2))
rootfs_blocks=$(($block_total-20480-20480))

dd if=head.img of=system.img 
dd if=boot.img of=system.img bs=512 seek=20480 count=20480
dd if=rootfs.img of=system.img bs=512 seek=40960 count=$rootfs_blocks
