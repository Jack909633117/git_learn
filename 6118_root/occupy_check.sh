#!/bin/sh

case `lsof | grep $1` in
*$1)
    echo "[$1] is busy !!"
    exit 0
    ;;
*)
    ;;
esac

exit 1
