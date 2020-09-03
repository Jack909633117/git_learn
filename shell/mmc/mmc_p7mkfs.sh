#!/bin/sh

mmcblk0p7MountPath="/mnt/devdisk/_static"
diskMountPath="/mnt/devdisk/static"

umount $diskMountPath
umount $mmcblk0p7MountPath

umount /dev/mmcblk0p7
sleep 0.1
mkfs.ext4 /dev/mmcblk0p7 <<EOF
y
EOF
