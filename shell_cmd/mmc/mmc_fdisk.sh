#!/bin/sh
fdisk-old /dev/mmcblk0 <<EOF
n
e
716800	#预留了10+10+30+300=350MB的空间

n

+300M
n

+300M
n


t
6
6
t
7
b
w
EOF
