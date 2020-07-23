#!/bin/sh

mmc_dev="/dev/mmcblk0"
mmc_mount_dev=$mmc_dev"p1"
mount_dir="/mnt/devdisk"

## help
if [ -z $1 ]; then
    echo -e "\nexample:  \n  $0 1 ## remount sd card\n  $0 2 ## umount sd card\n  $0 3 ## mkfs and mount sd card"
    echo -e "default mmc dev: $mmc_dev"
    echo -e "default mmc mount dev: $mmc_mount_dev"
    echo -e "return: 0 invaid, 1 success, 255 failed\n"
    exit 0
fi

## function
kill_pid()
{
    pidNum=`ps aux | grep -v grep | grep $1 | tr -s ' '| cut -d ' ' -f 2`
    if [ $pidNum ] ; then
        echo "-----> kill $1 $pidNum"
        kill $pidNum
    fi
}

kill_occupy()
{
#    kill_pid process_log
#    kill_pid process_upgrade
#    kill_pid process_record
    killall process_log
    killall process_upgrade
    killall process_record
    killall process_repair
}

restore_occupy()
{
    cd /usr/local/fw6118
    echo "-----> restore  process_log"
    ./process_log &
    # echo "-----> restore  process_upgrade"
    # ./process_upgrade &
    echo "-----> restore  process_record"
    ./check_sd.sh &
    echo "-----> restore  process_repair"
    ./process_repair &
    cd - 
}

mkfs_vfat()
{
fdisk $mmc_dev <<EOF
o
n
p
1


t
b
w
EOF
mkfs.vfat $mmc_mount_dev
}

sd_bakup()
{
    cd $mount_dir
    mkdir /tmp/RFIDDATA
    cp ./RFIDDATA/TOOLLIB* /tmp/RFIDDATA
    cd -
}

sd_install()
{
    cd $mount_dir
    mkdir ./video
    mkdir ./photo
    mv /tmp/RFIDDATA ./
    mkdir ./RFIDDATA
    mkdir ./static
    mkdir ./update
    cp /usr/local/fw6118/res/logo.jpg ./photo
    cd -
}

## remount sd card
if [ $1 ==  1 ]; then

    echo "param: 1"
    if [ -e $mmc_mount_dev ]; then
        kill_occupy
        umount $mount_dir
        sleep 0.5
        mount $mmc_mount_dev $mount_dir
        restore_occupy
        exit 1
    else
        echo "remount error: $mmc_mount_dev not exist !!"
        exit 255
    fi

## umount sd card
elif [ $1 ==  2 ]; then

    echo "param:2"
    if [ -e $mount_dir ]; then
        kill_occupy
        umount $mount_dir
#        restore_occupy
        exit 1
    else
        echo "umount error: $mount_dir not exist !!"
        exit 255
    fi

## mkfs and mount sd card
elif [ $1 ==  3 ]; then

    echo "param: 3"
    if [ -e $mmc_dev ]; then
        kill_occupy
        sd_bakup
        umount $mmc_mount_dev
        mkfs_vfat
        mount $mmc_mount_dev $mount_dir
        sd_install
        restore_occupy
        exit 1
    else
        echo "mkfs error: $mmc_dev not exist !!"
        exit 255
    fi

## unknow param
else

    echo -e "unknow param: $0 $1\n"
    exit 0

fi

