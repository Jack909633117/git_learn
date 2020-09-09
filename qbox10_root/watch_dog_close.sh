
# watch_dog_close  -> kill watch_dog
# watch_dog_close all  -> kill watch_dog + kill pppd + kill qbox10_main + kill node

exei2c="/usr/sbin/i2c_transfer"
param="/dev/i2c-1 0x51"

# reg
regEn="0x2A 1 1 2"
regWd="0x2D 1 1 2"
regPin="0x27 1 1 2"

# close
pidNum=`ps| awk '/watch_dog_run/{if($3 != "awk") print $1}'`
if [ $pidNum ] ; then
    echo "-----> kill watch_dog $pidNum"
    kill -9 $pidNum
fi

# reg clean
$exei2c $param $regPin 0x00
$exei2c $param $regEn 0x00

# $1 exist && $1 == "all"
if [ $1 -a $1 == "all" ] ; then

    if true ; then

        # close py_websocket
        pidNum=`ps | awk '/websocket_client/{if($3 != "awk") print $1}'`
        if [ $pidNum ] ; then
            echo "-----> kill py_websocket $pidNum"
            kill -9 $pidNum
        fi

        # close py_http
        pidNum=`ps | awk '/http_client/{if($3 != "awk") print $1}'`
        if [ $pidNum ] ; then
            echo "-----> kill py_http $pidNum"
            kill -9 $pidNum
        fi

        # close py_signalr_check
        pidNum=`ps | awk '/py_signalr_check/{if($3 != "awk") print $1}'`
        if [ $pidNum ] ; then
            echo "-----> kill py_signalr_check $pidNum"
            kill -9 $pidNum
        fi

        # close intercom
        pidNum=`ps | awk '/intercom/{if($3 != "awk") print $1}'`
        if [ $pidNum ] ; then
            echo "-----> kill intercom $pidNum"
            kill -9 $pidNum
        fi

        # close py_signalr
        pidNum=`ps | awk '/py_signalr/{if($3 != "awk") print $1}'`
        if [ $pidNum ] ; then
            echo "-----> kill py_signalr $pidNum"
            kill -9 $pidNum
        fi

        # close 4g_start
        pidNum=`ps | awk '/4g_start/{if($3 != "awk") print $1}'`
        if [ $pidNum ] ; then
            echo "-----> kill 4g_start $pidNum"
            kill -9 $pidNum
        fi

    fi

    # close qbox10_main
    pidNum=`ps | awk '/qbox10_main/{if($3 != "awk") print $1}'`
    if [ $pidNum ] ; then
        echo "-----> kill qbox10_main $pidNum"
        kill -9 $pidNum
    fi

fi

