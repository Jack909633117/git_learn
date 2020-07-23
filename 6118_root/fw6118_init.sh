
source /etc/profile

/root/test/start.sh

/root/watchdog.sh &

python /usr/local/fw6118/server/redis_data_init.pyc &
cd /usr/local/fw6118
# ./cpu_temp/himm 0x120300BC &
./mipi_init.sh
./hiplay -l ./res/logo.jpg -rtsp /usr/local/fw6118/rtspToH264 &
./dotcheck/RFID_init.sh
./insmod-key.sh
./process_upgrade &
./process_log &
./process_position &
./process_netcheck &
./process_rtsp2rtmp "rtsp://192.168.80.1:554/h264minor" &

python /usr/local/fw6118/camera_ptz/ptz.pyc &
python /usr/local/fw6118/dotcheck/ToolDotCheck.pyc &

./qt5 rtsp://192.168.80.1:554/h264minor &

python /usr/local/fw6118/server/server.pyc -m 1 -a &
cd -

/root/watchdogRegist.sh &

#repair mp4; record
cd /usr/local/fw6118/
./process_repair &
./check_sd.sh &
cd -
#wifi
cd /usr/local/fw6118/wifi
./wifi.sh &
cd -
sleep 10
#mode
cd /usr/local/fw6118/ap
./mode.sh &
cd - 
#4g
#cd /usr/local/fw6118/ap
#./4g.sh
#cd -
#4g
#cd /usr/local/fw6118/ap
#./4g.sh
#sleep 1
#./ap.sh
#cd -

/root/checkProcess.sh &

