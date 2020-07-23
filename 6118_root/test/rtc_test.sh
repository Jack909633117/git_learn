
ret=`hwclock -r`

date "2020-06-10 12:30:30" > /dev/null

hwclock -w > /dev/null

ret2=`hwclock -r`

# result
if [ "$ret" == "$ret2" ];then
    exit 1
else
    exit 0
fi

