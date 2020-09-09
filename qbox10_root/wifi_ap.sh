while [ true ]                                                                  
do                                                                              
	line=$(ifconfig | grep ppp0)                                            
	echo $line                                                              
	if [ -n "$line" ]; then                                                 
		echo "Start AP service."                                        
		/usr/share/wl18xx/ap_start.sh                                   
		exit 1                                                          
	fi                                                                      
	sleep 0.5                                                               
done
