#!/bin/bash 
function ubi_usage ()
{
	echo "Usage: ${selfname}  Type size version"
	echo "  Type            File system type "
	echo "  size            File system size "
	echo "  version         File system version "
	echo "Example:"
	echo "  ${selfname} ubi 240 1.0.0"
	echo ""
	exit 0
}
function update_usage ()
{
	echo "Usage: ${selfname} update OldVersion NewVersion size"
	echo "  update            必须为update "
	echo "  OldVersion        旧版本号 "
	echo "  NewVersion        新版本号 "
	echo "  size              升级固件容量大小 "
	echo "Example:"
	echo "  ${selfname} update 1.0.0 1.2.0 20 "
	echo ""
	exit 0
}
selfname=$(basename $0)

./tools/empty_deal.sh delect
./tools/delect.sh

echo $#

if [ $1 == "ubi" ]; then
	if [ "$#" != 3 ]; then
		ubi_usage;
	fi
	./tools/py_deal.sh pyc_deal rootfs/usr/local/fw6118/camera_ptz
	./tools/py_deal.sh pyc_deal rootfs/usr/local/fw6118/dotcheck
	./tools/py_deal.sh pyc_deal rootfs/usr/local/fw6118/server
	./tools/version.sh $3
	./tools/pc/ubi_sh/mkubiimg.sh hi3516dv300 2k 128k rootfs $2M ./tools/pc 0

	./tools/py_deal.sh py_recover rootfs/usr/local/fw6118/camera_ptz
	./tools/py_deal.sh py_recover rootfs/usr/local/fw6118/dotcheck
	./tools/py_deal.sh py_recover rootfs/usr/local/fw6118/server
	./tools/py_deal.sh clean
fi

if [ $1 == "update" ]; then
	if [ "$#" != 4 ]; then
		update_usage;
	fi
	./tools/update.sh $2 $3 $4
	./tools/empty_deal.sh add
else
	./tools/empty_deal.sh add
	#update_usage
fi


