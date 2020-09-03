1、编译安装flex

#libpcap 1.1要求flex必须在2.4.6及以上
wget http://prdownloads.sourceforge.net/flex/flex-2.5.36.tar.gz
tar -xzvf flex-2.5.36.tar.gz
cd flex-2.5.36
./configure --prefix=/usr/local/libpcap
#./configure --prefix=/usr/local/libpcap_hixi200/ --host=arm-himix200-linux --with-pcap=linux
make -j
sudo make install

2、编译安装bison

#libpcap要求同时安装flex赫bison
wget http://ftp.gnu.org/gnu/bison/bison-2.4.tar.gz
tar -xzvf bison-2.4.tar.gz
./configure --prefix=/usr/local/libpcap
make -j && make install

3、编译安装libpcap

wget http://www.tcpdump.org/release/libpcap-1.1.1.tar.gz
tar -xzvf libpcap-1.1.1.tar.gz
./configure --prefix=/usr/local/libpcap
make -j
make install

4、测试程序(root user)
sudo ./main


5、libpcap交叉编译 
export CC=arm-himix100-linux-gcc
export STRIP=arm-himix100-linux-strip
export AR=arm-himix100-linux-ar
export RANLIB=arm-himix100-linux-ranlib
export OBJCOPY=arm-himix100-linux-objcopy

./configure --host=arm-himix100-linux --prefix=/usr/local/libpcap_arm --with-pcap=linux
#./configure  --host=arm-himix200-linux --prefix=/usr/local/libpcap_hixi200 --with-pcap=linux
make
sudo make install

#gcc -o network_monitor ./main.c -L /usr/local/libpcap/lib/ -I /usr/local/libpcap/include -I /usr/local/libpcap/include/pcap -lpthread -lpcap 
#hisi3516dv300
arm-himix200-linux-gcc -o network_monitor ./main.c -L /usr/local/libpcap_hixi200/lib/ -I /usr/local/libpcap_hixi200/include -I /usr/local/libpcap_hixi200/include/pcap  -lpthread -lpcap 

6、带宽自适应

程序功能跟wireshake一致，主要是监控网络情况，那么可以根据预设的负载和实际监控的传输情况来调节网络配置。

1) 比如设定了需要传输一路海康主码流的视频，1080P，码率4Mbps,那么理论上网速应该就是4Mbit/s（500kbyte/s），
当监控的实际情况小于20%的预设值，即实时网速低于400kbyte/s，图像会出现卡顿，马赛克等异常。此时需要降低摄像头码率到带宽可以承载的范围内。
多个负载的情况，依次累加。

2) 程序除了实时统计网络传输速度，还会记录数据包时间戳间隔，以200ms为临界点，数据包间隔大于200ms时返回的status为-1，几乎传输不了视频和音频。
音频数据包时间戳范围在20ms ± 20%，视频数据包时间戳范围在1~10Ms ± 20%, 如果不能快速恢复，数据传输必然出现了故障。

3) 程序上报线程report_routine里作了超时检测，如果数据包时间戳间隔超过10s了，要么是停止了传输，要么是网络出现异常，堵塞或者断网了。

