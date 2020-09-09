#dd if=/dev/zero of=devdisk.img bs=1M count=20
#losetup /dev/loop0 /mnt/devdisk.img         #将镜像与loop0建立连接
#mount /mnt/devdisk.img /mnt/devdisk     #挂载挂载点g_acm_ms
#modprobe g_mass_storage file=/dev/mmcblk0p3 stall=0 removable=1
#下面的可用网口,windows
#modprobe g_ether.ko 
#ifconfig usb0 192.168.10.2
#下面的是挂载linux下使用的，主要是调试使用
#modprobe g_acm_ms.ko file=/dev/mmcblk0p3 ro=0 luns=1 stall=0 removable=11 iSerialNumber=3000111
#下面的可用串口和存储
#modprobe g_acm_ms.ko file=/dev/mmcblk0p6,/dev/mmcblk0p7 ro=0,1 luns=2 stall=0 removable=1,1 iSerialNumber=3000111
modprobe g_acm_ms.ko file=/dev/mmcblk0p6,/mnt/devdisk/_static/.image ro=0,1 luns=2 stall=0 removable=1,1 iSerialNumber=3000111
#下面的可用串口和网口,linux
#sleep 5
#modprobe g_cdc.ko 
#modprobe g_multi.ko file=/dev/mmcblk0p6,/dev/mmcblk0p7 ro=0,1 luns=2 stall=0 removable=1,1 iSerialNumber=3000111
#ifconfig usb0 192.168.0.6
#insmod /lib/modules/3.6.9/kernel/drivers/usb/gadget/g_mass_storage.ko file=/mnt/devdisk.img stall=0 removable=1
#insmod /lib/modules/3.6.9/kernel/drivers/usb/gadget/g_acm_ms.ko file=/mnt/devdisk.img luns=1 ro=0 stall=0 removable=1 iSerialNumber=3000111 iProduct=zhdgnss iManufacturer=zhd_survey
#insmod /lib/modules/3.6.9/kernel/drivers/usb/gadget/g_multi1.ko file=/mnt/devdisk.img stall=0 removable=1 iSerialNumber=3000111 iProduct=zhdgnss iManufacturer=zhd_survey
#insmod /lib/modules/3.6.9/kernel/drivers/usb/gadget/g_multi.ko file=/mnt/devdisk.img luns=1 ro=0 stall=0 removable=1 iSerialNumber=3000111 iProduct=zhdgnss iManufacturer=zhd_survey.
