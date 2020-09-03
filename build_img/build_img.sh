#!/bin/sh

if [ -z $3 ]; then
    echo -e "格式:\n  $0 [版本号] [主文件系统大小(M)] [0:铁路版/1:电力版]"
    echo -e "example:\n  $0 1.0.0 280 0"
    exit 0
fi

if [ $3 == "1" ]; then
    IMG_NAME="Qbox10_System_V$1_ELECTRIC.img"
else
    IMG_NAME="Qbox10_System_V$1.img"
fi

./empty_deal.sh delect

#写入更新时间
./update_time.sh

#写入版本号
configFilePath="./rootfs/usr/local/qbox10/config/system.conf"
line="sys-version        =$1 # ($1)"
sed -i "/^sys-version/c $line" $configFilePath

#写入server_type,的电力服务器
line="sys-server_type    =$3 # (0:railway;1:electric)"
sed -i "/^sys-server_type/c $line" $configFilePath

#往/boot/version写入版本号
bootVersionFilePath="./boot/version"
if [ -e $bootVersionFilePath ] ; then
    echo $1 > $bootVersionFilePath
fi

#总大小: 预留(10M) + boot(FAT32 10M) + rootfs_min(ext4 30M) + rootfs(ext4 ???M)
tar_img=$IMG_NAME #"system.img"
#10M
boot_img="boot.img"
#30M
rootfs_min_img="rootfs_min.img"
#$2M
rootfs_img="rootfs.img"

#总大小: 预留(10M) + boot(FAT32 10M) + rootfs_min(ext4 30M) + rootfs(ext4 ???M)
totalSize=$((10+10+30+$2))

#创建分区文件
dd if=/dev/zero of=./$tar_img bs=1M count=$totalSize
dd if=/dev/zero of=./$boot_img bs=1M count=10
dd if=/dev/zero of=./$rootfs_min_img bs=1M count=30
dd if=/dev/zero of=./$rootfs_img bs=1M count=$2

#tar_img要准备好每个人的坑的位置
fdisk $tar_img << EOF
n
p
1
20480
+10M
n
p
2
40960
+30M
n
p
3
102400

p
w
EOF
#格式化各分区
mkfs.vfat -n "boot" ./$boot_img << EOF
Y
EOF
mkfs.ext4 -L "rootfs_min" ./$rootfs_min_img << EOF
Y
EOF
mkfs.ext4 -L "rootfs" ./$rootfs_img << EOF
Y
EOF

#创建挂载目录
mkdir ./boot_temp
mkdir ./rootfs_min_temp
mkdir ./rootfs_temp

#挂载
sudo mount $boot_img ./boot_temp
sudo mount $rootfs_min_img ./rootfs_min_temp
sudo mount $rootfs_img ./rootfs_temp

#拷贝
sudo cp ./boot/* ./boot_temp -rf
sudo cp ./rootfs_min/* ./rootfs_min_temp -rf
sudo cp ./rootfs/* ./rootfs_temp -rf

#解除挂载
sudo umount ./boot_temp
sudo umount ./rootfs_min_temp
sudo umount ./rootfs_temp

#删除挂载目录
sudo rm -rf ./boot_temp
sudo rm -rf ./rootfs_min_temp
sudo rm -rf ./rootfs_temp

#按地址把 boot rootfs_min rootfs 拷贝到 tar_img
dd if=$boot_img of=$tar_img bs=1M seek=10 conv=notrunc
dd if=$rootfs_min_img of=$tar_img bs=1M seek=20 conv=notrunc
dd if=$rootfs_img of=$tar_img bs=1M seek=50 conv=notrunc

#删除多余文件
rm $boot_img $rootfs_min_img $rootfs_img

./empty_deal.sh add
#加密
./Encryption/Encrypt_For_Ubuntu $tar_img E
