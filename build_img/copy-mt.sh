#!/bin/sh
mkdir boot_tmp
mkdir rootfs_tmp
sudo mount -t vfat boot.img boot_tmp
sudo mount -t ext4 rootfs.img rootfs_tmp
sudo cp -rf ./boot-mt/* boot_tmp
sudo cp -rf ./rootfs/* rootfs_tmp
sync
sudo umount ./boot_tmp
sudo umount ./rootfs_tmp

sudo rm -rf ./boot_tmp
sudo rm -rf ./rootfs_tmp
