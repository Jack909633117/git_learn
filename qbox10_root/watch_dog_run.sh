
# watch_dog_run  -> watch_dog
# watch_dog_run all  -> watch_dog + check qbox10_main + check node

logFile="/mnt/devdisk/static/log/watch_dog.log"

exeI2c="/usr/sbin/i2c_transfer"
param="/dev/i2c-1 0x51"

# reg
regEn="0x2A 1 1 2"
regWd="0x2D 1 1 2"
regPin="0x27 1 1 2"

# 0x7C : 31*4 = 124 sec
# 0x40 : 16*4 = 64 sec
# 0x20 : 8*4 = 32 sec
# 0x0C : 3*4 = 12 sec
wdTime=0x40

# wd period 30 second
wdSleep=30

# init
echo "RTC watch dog init"
$exeI2c $param $regEn 0x01
$exeI2c $param $regWd $wdTime
$exeI2c $param $regPin 0x04

# flag
checkAll="no"
if [ $1 -a $1 == "all" ] ; then
    checkAll="yes"
fi

# main loop
while : ; do
    
    sleep $wdSleep
    
    # feed dog
    $exeI2c $param $regWd $wdTime > /dev/null
    
    sleep 0.2
    
    # ----- here to check process -----
    if [ $checkAll == "yes" ] ; then
        
        # check qbox10_main
        result=`ps | awk '/qbox10_main/{if($3 != "awk") print $1}'`
        if [ ! $result ] && [ -e "/usr/local/qbox10/qbox10_main" ] ; then
            dateInfo=`date +%Y-%m-%d-%H:%M:%S`
            echo "[$dateInfo] [watch_dog] qbox10_main restart now ..."
            echo "[$dateInfo] [watch_dog] qbox10_main restart" >> $logFile
            /usr/local/qbox10/qbox10_main &
        fi
        
        if false ; then

            sleep 0.2
            
            # check py_websocket
            result=`ps | awk '/websocket_client/{if($3 != "awk") print $1}'`
            if [ ! $result ] && [ -e "/usr/bin/node-v4.2.6" ] && [ -e "/usr/local/qbox10/py_websocket/websocket_client.py" ] ; then
                dateInfo=`date +%Y-%m-%d-%H:%M:%S`
                echo "[$dateInfo] [watch_dog] websocket_client.py restart now ..."
                echo "[$dateInfo] [watch_dog] websocket_client.py restart" >> $logFile
                /root/py_websocket.sh
            fi
        
        fi
    fi

done

