#!/bin/sh

# --- remove usb driver ---
# /root/rmmod_usb.sh

# --- mmcblk0p7 ---
mmcblk0p7MountPath="/mnt/devdisk/_static"
diskPath="$mmcblk0p7MountPath/.image"
diskMountPath="/mnt/devdisk/static"

umount $diskMountPath
umount $mmcblk0p7MountPath

umount /dev/mmcblk0p7
sleep 0.1
mkfs.ext4 /dev/mmcblk0p7 <<EOF
y
EOF

# --- static disk ---
mkdir $mmcblk0p7MountPath
mkdir $diskMountPath

if mount -t ext4 /dev/mmcblk0p7 $mmcblk0p7MountPath && echo ok >> /dev/null ; then

sleep 0.1

# dd if=/dev/zero of=$diskPath bs=1M count=6114
fallocate -l 6G $diskPath

# mkfs
mkfs.vfat $diskPath <<EOF
y
EOF
echo "fsck.fat $diskPath"
fsck.fat $diskPath

mkdosfs -n static $diskPath

else
    echo "mount /dev/mmcblk0p7 failed !!"
fi
