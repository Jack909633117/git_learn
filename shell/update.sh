if [ -z $2 ]; then
	echo "Fail:  You must specify 2 parameter!"
	echo "Usage: $0 OldVersion NewVersion size"
	echo "	Example: $0 1.0.0 1.2.0 20"
	exit 1
fi

IMG_NAME="fw6118_update_V$1_to_V$2.bin"

#写入当前版本号到cmd.sh
line="version=\"$2\""
sed -i "/^version/c $line" ./update/cmd.sh

#准备版本升级文件
rm -rf ./update/rootfs # 清除旧文件，确保不干扰新的文件
./tools/git_export.sh copy $(git rev-parse V$1) $(git rev-parse V$2) ./update
# 保存需要打包的rootfs和cmd.sh
mkdir -p ./tmp
mv ./update/rootfs ./tmp
cp ./update/cmd.sh ./tmp
# 清空./update目录
rm -rf ./update/*
# 恢复./update目录
mv ./tmp/rootfs ./update
cp ./tmp/cmd.sh ./update
# 处理python文件，保护代码
./tools/py_deal.sh pyc_deal update/rootfs/usr/local/fw6118/camera_ptz
./tools/py_deal.sh pyc_deal update/rootfs/usr/local/fw6118/dotcheck
./tools/py_deal.sh pyc_deal update/rootfs/usr/local/fw6118/server

#禁止升级时替换主配置文件
#rm ./upgrade/rootfs/usr/local/fw6118/config/system.db
#删除配置文件，保持设备最后一次配置的参数
rm ./update/rootfs/usr/local/fw6118/ap/ap.conf
rm ./update/rootfs/usr/local/fw6118/ap/mode.conf
rm ./update/rootfs/usr/local/fw6118/ap/sta.conf
rm ./update/rootfs/usr/local/fw6118/wifi/wifi.conf
rm ./update/rootfs/usr/local/fw6118/config/server.conf
rm ./update/rootfs/usr/local/fw6118/camera_ptz/camera.ini

#创建分区文件
dd if=/dev/zero of=./$IMG_NAME bs=1M count=$3

#格式化分区
mkfs -t ext4 ./$IMG_NAME <<EOF
Y
EOF

./tools/empty_deal.sh delect

#挂载和拷贝
mkdir ./mount_temp
sudo mount ./$IMG_NAME ./mount_temp
sudo cp ./update/* ./mount_temp/ -R
sudo umount ./mount_temp
sudo rm ./mount_temp -rf

./tools/empty_deal.sh add

#删除中间文件
rm -rf ./update/*
cp ./tmp/cmd.sh ./update/cmd.sh

rm -rf ./tmp
./tools/py_deal.sh clean
