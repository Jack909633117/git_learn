
himm 0x120E0000 0x200FF > /dev/null
himm 0x120E0000 0x201FF > /dev/null
himm 0x120E001C 0x1 > /dev/null
ret=`himm 0x120E002C 0xFFFF`
himm 0x120E0020 0x1 > /dev/null

# value range
val=0
# 24V: 24/0.030078
valMax=798
# 10V: 10/0.030078
valMin=333

# find val
hit=false
for i in $ret ; do
    if [[ "$hit" == "false" ]] && [[ "$i" == "0x120E002C:" ]];then
        hit=true
    elif [ "$hit" == "true" ];then
        # hex to int
        val=`printf %d $i`
        break
    fi
done

# compare
if [ $val -gt $valMax ];then
#    echo "over max !! [$val] > [$valMax]"
    exit 1
elif [ $val -lt $valMin ];then
#    echo "less than min !! [$val] < [$valMin]"
    exit 2
else
#    echo "fiti [$val]"
    exit 0
fi

