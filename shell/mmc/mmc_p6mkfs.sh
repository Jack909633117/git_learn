#!/bin/sh

umount /dev/mmcblk0p6
mkfs.vfat /dev/mmcblk0p6 <<EOF
y
EOF
echo "fsck.fat /dev/mmcblk0p6"
fsck.fat /dev/mmcblk0p6
mkdosfs -n update /dev/mmcblk0p6
