#!/bin/sh

appList="hiplay"
pidWd=/tmp/wd

if [ -e $pidWd ]; then
    for i in $appList ; do
        ret=`ps | grep -v grep | grep $i | awk '{print $1}'`
        echo "[WDREG] find $i pid $ret"
        echo "$i" > $pidWd/$ret
    done
fi
