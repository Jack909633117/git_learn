#!/bin/sh

# example: gpio_init 10 out
gpio_init(){
    cd /sys/class/gpio
    if [ -e ./gpio${1} ]; then
        echo $1 > ./unexport
    fi
    echo $1 > ./export
    echo $2 > ./gpio${1}/direction 
    cd -
}

# example: gpio_set 10 1
gpio_set(){
    echo $2 > /sys/class/gpio/gpio${1}/value
}

# example: gpio_get 10 1
gpio_get(){
    ret=`cat /sys/class/gpio/gpio${1}/value`
    return $ret
}

# motor
gpio_init 95 out
# led Red
gpio_init 66 out
# led Green
gpio_init 9 out
# led Blue
gpio_init 10 out

while : ; do
    gpio_set 95 1
    gpio_set 66 0
    sleep 0.5
    gpio_set 66 1
    gpio_set 9 0
    sleep 0.5
    gpio_set 9 1
    gpio_set 10 1
    sleep 0.5
    gpio_set 10 0
    gpio_set 95 0
    sleep 0.5
done
