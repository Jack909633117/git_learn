#如果已定义KERNELRELEASE，说明是由内核构造系统调用的
#可以利用内建语句
#demo_chrdevmodule-objs:=module
#CFLAGS = -Wall
# ifneq ($(KERNELRELEASE),)

#     #此时由内核构造系统调用
# else
    #定义并记录内核源码路径
    #KERNELDIR = /mysdb/at91/linux-at91-3.6.9/
# obj-m +=io_keyboard.o
# KERNELDIR = /mysdb/fw61178/linux-4.9.37/
# #记录当前工程目录
# PWD := $(shell pwd)
# CC = arm-himix200-linux-gcc

# default:
# 	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
# 	@rm -rf *.o .t* .m* .*.cmd *.mod.c *.order *.symvers

# # endif

# clean:
#  #   rm -rf *.ko *.o .t* .m* .*.cmd *.mod.c *.order *.symvers  
#     $(MAKE) -C $(KERNELDIR) M=$(PWD) clean 

# obj-m:=io_keyboard.o
# KDIR = /mysdb/fw61178/linux-4.9.37/  #这里需要你根据自己的实际的linux源码放的位置来设置

# all:
#     $(MAKE) -C $(KDIR) SUBDIRS=$(shell pwd) modules ARCH=arm CROSS_COMPILE=arm-himix200-linux-
# # clean:
#     rm -rf io_keyboard*.o

#make ARCH=arm CROSS_COMPILE=arm-himix200-linux-

obj-m := io_keyboard.o
KERNEL_PATH := /mysdb/fw61178/linux-4.9.37/
PWD := $(shell pwd)
all:  
	make -j4 -C $(KERNEL_PATH) M=$(PWD) modules 
	cp io_keyboard.ko /mysdb/fw61178/mynfs/ 
clean:  
i	make -C $(KERNEL_PATH) M=$(PWD) clean  
