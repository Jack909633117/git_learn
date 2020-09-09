#!/bin/sh

mmcblk0p7MountPath="/mnt/devdisk/_static"
diskPath="$mmcblk0p7MountPath/.image"
diskMountPath="/mnt/devdisk/static"

echo "static check ..."

#===== step 1 : is fat32 ? ======
isFat32=`hexdump -C -n 512 -s 0 $diskPath | grep -c 'FAT32'`

if [ $isFat32 -eq 0 ]; then
    echo "$diskPath is not fat32 !!"
	umount $diskMountPath
    /etc/init.d/mmc/mmc_staticmkfs.sh
    mount $devName $diskMountPath
    exit 0
fi
