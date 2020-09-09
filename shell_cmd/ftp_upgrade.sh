#!/bin/sh

localPath="/root"

targetPkgDownloadPath="/mnt/devdisk/update/upgrade"
tmpPath="/tmp"

#----- shell path -----

# wget path
wget="wget"
# wput path
wput="wput"
# md5sum path
md5sum="md5sum"
# upgrade
upgrade="upgrade"

#----- ftp download/upload -----

# ftp ip & port
ftpIp="59.42.52.138"
ftpIpBak="mtxy.hi-target.com.cn"
ftpPort="15009"
# ftp user
ftpUser="M20DEV"
# ftp log path
ftpLog="log"

# [ ftp server folder structure ]
# root path ............... read only
# |-update.txt ............ read only
# |-cmd_file .............. read only
# |-pkg_file .............. read only
# |-... ................... read only
# |-.. .................... read only
# |-. ..................... read only
# |-log ................... read/append
# |  |-xxxx1.log .......... read/append
# |  |-xxxx2.log .......... read/append
# |  |-... ................ read/append
# |  |-.. ................. read/append
# |  |-. .................. read/append

# download $srcPath $distPath
# default remote folder /m20
# example: download "update.txt" $localPath"/update.txt"
download()
{
    # -t [retry] -T [timeout] -q [quit]
    $wget -t 2 ftp://$ftpUser@$ftpIp:$ftpPort/$1 -O $2 -o $tmpPath/wget.log
    # $wget -t 2 ftp://$ftpUser@$ftpIp/$1 -O $2 -o $tmpPath/wget.log

    if cat $tmpPath/wget.log | grep '100%' > /dev/null
    then
        rm $tmpPath/wget.log
    else
        echo "wget backup ip $ftpIpBak"
        $wget -t 2 -q ftp://$ftpUser@$ftpIpBak:$ftpPort/$1 -O $2
        # $wget -t 2 -q ftp://$ftpUser@$ftpIpBak/$1 -O $2
    fi
}

# upload $srcPath $distPath
# default remote folder /m20/log
# example: upload $localPath"/order.txt" "log/123.log"
upload()
{
    # -q [quit]
    $wput -t 2 $1 ftp://$ftpUser@$ftpIp:$ftpPort/$2 -o $tmpPath/wput.log
    # $wput -t 2 5 $1 ftp://$ftpUser@$ftpIp/$2 -o $tmpPath/wput.log

    if cat $tmpPath/wput.log | grep 'Transfered' > /dev/null
    then
        rm $tmpPath/wput.log
    else
        echo "wput backup ip $ftpIpBak"
        $wput -t 2 -q $1 ftp://$ftpUser@$ftpIpBak:$ftpPort/$2
        # $wput -t 2 -q $1 ftp://$ftpUser@$ftpIpBak/$2
    fi
}

#----- deal with update.txt -----

# local order
localOrder=$localPath"/ftp_upgrade_order.conf"
# download update.txt
localUpdate=$tmpPath"/ftp_upgrade_update.txt"
# devnum.conf
devnum=$localPath"/ftp_upgrade_devnum.conf"

# target type: "cmd" or "pkg"
targetType="cmd"
# target file
targetFile=""
# target file md5
targetFileMd5=""
# target hit flag
#   0/invaild
#   1/refresh local order, do nothing
#   2/hit this device, download targetFile and upgrade ..
targetHit=0

# dev number
devId=14770000
if [ -e $devnum ]; then
    devId=`cat $devnum`
fi

# history order
order="20190601083000"
if [ -e $localOrder ]; then
    tmp=`cat $localOrder`
    if [ $tmp -gt $order ]; then
        order=$tmp
    fi
fi
echo $order > $localOrder

# check update.txt
# example: upgrade `sed -n '$p' update.txt`
# return: 0/invaild
#         1/refresh local order, do nothing
#         2/hit this device, download targetFile and upgrade ..
check_update()
{
    # echo "< ftpUpgrade > [$*]"
    # order compare
    if [ $# -gt 3 ] && [ $1 -gt $order ]; then
        echo "< ftpUpgrade > order: $1 > $order"
        # invaild type ?
        if [ $2 == "cmd" ] || [ $2 == "pkg" ]; then
            order=$1
            targetType=$2
            targetFile=$3
            targetFileMd5=$4
            # load devId
            if [ -e $devnum ]; then
                devId=`cat $devnum`
            fi
            # device list ?
            if [ $# -gt 4 ]; then
                # look for devId
                for i in $* ; do
                    if [ $i == $devId ]; then
                        targetHit=2
                        return 2
                    fi
                done
                targetHit=1
                return 1
            else
                targetHit=2
                return 2
            fi
        else
            echo "< ftpUpgrade > type: unknow $2"
            targetHit=0
            return 0
        fi
    else
        echo "< ftpUpgrade > order: $1 <= $order"
        targetHit=0
        return 0
    fi
}

# update
# example: update
do_update()
{
    if [ $targetHit -gt 0 ]; then

        localTime=`date +%Y%m%d%H%M%S`
        logFileName="$order-$devId-"

        # refresh order.conf
        echo "< ftpUpgrade > refresh order.conf [$order]"
        echo $order > $localOrder

        if [ $targetHit -gt 1 ]; then

            if [ $targetType == "pkg" ] && [ -e $targetPkgDownloadPath ]; then
                # download target
                echo "< ftpUpgrade > download target [$targetType $targetFile]"
                download "$targetFile" $targetPkgDownloadPath"/$targetFile"
                targetFile=$targetPkgDownloadPath"/$targetFile"
            elif [ $targetType == "cmd" ] && [ -e $tmpPath ]; then
                # download target
                echo "< ftpUpgrade > download target [$targetType $targetFile]"
                download "$targetFile" $tmpPath"/$targetFile"
                targetFile=$tmpPath"/$targetFile"
            fi

            # download success ?
            if [ -e $targetFile ]; then
                # md5 calculate
                tmpMd5=`$md5sum $targetFile`
                tmpMd5=${tmpMd5%\ *}
                # md5 compare
                echo "< ftpUpgrade > md5: $tmpMd5 -- $targetFileMd5"
                if [ $tmpMd5 == $targetFileMd5 ]; then
                    if [ $targetType == "cmd" ]; then
                        # result: cmd
                        logFileName=$logFileName"cmd.log" && echo "[$localTime]" > $tmpPath"/$logFileName"
                        # run cmd
                        echo "< ftpUpgrade > cmd run now ..."
                        chmod a+x $targetFile
                        $targetFile >> $tmpPath"/$logFileName"
                    elif [ $targetType == "pkg" ]; then
                        # result: pkg
                        logFileName=$logFileName"pkg.log" && echo "[$localTime]" > $tmpPath"/$logFileName"
                        # upgrade
                        echo "< ftpUpgrade > upgrade now ..."
                        $upgrade -c -d Qbox10 -vf /usr/local/qbox10/config/system.conf -nf /usr/local/qbox10/config/system.conf >> $tmpPath"/$logFileName"
                        if [ $? -eq 1 ]; then
                            # upload result
                            upload "$tmpPath/$logFileName" "$ftpLog/$logFileName"
                            echo "< ftpUpgrade > upload: $logFileName done" && rm "$tmpPath/$logFileName"
                            # reboot and upgrade
                            reboot && sleep 30
                        fi
                    else
                        # result-err: type
                        logFileName=$logFileName"err-type.log" && echo "[$localTime]" > $tmpPath"/$logFileName"
                        echo "< ftpUpgrade > err: unknow type $targetType"
                    fi
                else
                    # result-err: md5
                    logFileName=$logFileName"err-md5.log" && echo "[$localTime]" > $tmpPath"/$logFileName"
                    echo "< ftpUpgrade > err: md5"
                fi
            else
                # result-err: download
                logFileName=$logFileName"err-download.log" && echo "[$localTime]" > $tmpPath"/$logFileName"
                echo "< ftpUpgrade > err: download"
            fi
        else
            # result: ignore
            logFileName=$logFileName"ignore.log" && echo "[$localTime]" > $tmpPath"/$logFileName"
            echo "< ftpUpgrade > ignore"
        fi
        # clear targetFile
        rm $targetFile -rf
        # upload result
        upload "$tmpPath/$logFileName" "$ftpLog/$logFileName"
        echo "< ftpUpgrade > upload: $logFileName done" && rm "$tmpPath/$logFileName"
    fi
    # clear targetHit
    targetHit=0
}

#----- main loop -----

# detect period (sec)
delay1=15
delay2=45

while : ; do

    sleep $delay1

    # download update.txt
    download "update.txt" $localUpdate

    # check update.txt
    check_update `sed -n '$p' $localUpdate`

    # do update
    do_update

    sleep $delay2
done
