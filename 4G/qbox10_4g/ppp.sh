echo "$1"
if [ -z $1 ]; then
        echo "Fail: You must specify which mode for internet!!!"
        exit 1
    fi
pppd call  $1 &
