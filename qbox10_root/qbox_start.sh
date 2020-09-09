#!/bin/sh
num=0                                 
while [[ "$num" != 10 ]]              
do
	ld_library_check=$(printenv | grep LD_LIBRARY_PATH)
	if [ $ld_library_check ]; then
		/usr/local/qbox10/qbox10_main &
		let "num = 10" 
		exit 0
	fi
	sleep 0.1
	let "num += 1"
done
