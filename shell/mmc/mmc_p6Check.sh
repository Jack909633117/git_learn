#!/bin/sh

devName="/dev/mmcblk0p6"

echo "mmcblk0p6 check ..."

#===== step 1 : is fat32 ? ======
isFat32=`hexdump -C -n 512 -s 0 $devName | grep -c 'FAT32'`

if [ $isFat32 -eq 0 ]; then
    echo "$devName is not fat32 !!"
	umount /mnt/devdisk/update
    /etc/init.d/mmc/mmc_p6mkfs.sh
    mount $devName /mnt/devdisk/update
    exit 0
fi

#===== step 2 : fstab mount success ? =====

#isMount=`df | grep -c $devName`

#if [ $isMount -eq 0 ]; then
#    echo "$devName haven't been mount !!"
#    /etc/init.d/mmc/mmc_p6mkfs.sh
#    mount -t vfat $devName /mnt/devdisk/update
#    exit 0
#fi

