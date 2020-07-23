#!/bin/sh

# 用户注册: 以pid命名,在/tmp/wd内创建文件,并写入自己的进程名称

pathWd=/tmp/wd
pidHistory=""
pathLog="/mnt/devdisk/watchdog.log"

# pid_match [target] [list...]
# example: pid_match 3 1 2 3 4 5
# return: 0/not found  1/found
pid_match()
{
    hit=0
    for j in $@ ; do
        if [ $1 == $j ]; then
            if [ $hit == "0" ]; then
                hit=1
            else
                return 1
            fi
        fi
    done
    return 0
}

pid_check()
{
    # path check
    if [ ! -e $pathWd ]; then
        mkdir $pathWd -p
    fi

    pidReg=`ls $pathWd`
    pidExist=`ls /proc`
    time=`date`
    log="[$time]"

    pidLost=0
    pidTmp=""

    for i in $pidReg ; do

        # is add ??
        pid_match $i $pidHistory
        if [ $? != "1" ]; then
            # exist ??
            pid_match $i $pidExist
            if [ $? != "1" ]; then
                echo "[WD] remove $i now ... [add not exist]"
                rm $pathWd/$i
                continue
            fi
        fi

        # check exist ...
        pid_match $i $pidExist
        if [ $? != "1" ]; then
            # log add
            info=`cat $pathWd/$i`
            echo "[WD] pid $i $info not exist"
            log="$log <pid $i info $info>"
            # flag
            pidLost=1
            # clean
            rm $pathWd/$i
            continue
        fi

        pidTmp="$pidTmp$i "

    done

    if [ $pidLost == "1" ]; then
        # wait halt
        echo "[WD] wait halt ..."
        sleep 10
        # write log
        echo $log >> $pathLog
        # try to reboot ...
        echo "[WD] wait reboot ..."
        reboot
        sleep 60
    fi

    # backup pidHistory
    pidHistory=$pidTmp

}


## ----- start ----- ##


# watch dog init (period 20s)
himm 0x12050000 0x03938700 >> /dev/null

# watch dog start
himm 0x12050008 0x3 >> /dev/null

# clear old log
rm /mnt/devdisk/error.log

# main loop
while : ; do

    pid_check

    # feed dog
    himm 0x1205000C 0x0 >> /dev/null

    sleep 10

done

# watch dog stop
himm 0x12050008 0x0 >> /dev/null
