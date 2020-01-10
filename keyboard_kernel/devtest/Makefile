#如果已定义KERNELRELEASE，说明是由内核构造系统调用的
#可以利用内建语句
#demo_chrdevmodule-objs:=module
CC = arm-himix200-linux-gcc
#CFLAGS = -Wall
ifneq ($(KERNELRELEASE),) 
    obj-m +=io_keyboard.o
    #此时由内核构造系统调用
else 
    #定义并记录内核源码路径
    #KERNELDIR = /mysdb/at91/linux-at91-3.6.9/
	KERNELDIR = /mysdb/fw61178/linux-4.9.37/
    #记录当前工程目录
    PWD := $(shell pwd)

default: 
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules 
	@rm -rf *.o .t* .m* .*.cmd *.mod.c *.order *.symvers  
 
endif
 
clean:
 #   rm -rf *.ko *.o .t* .m* .*.cmd *.mod.c *.order *.symvers  
    $(MAKE) -C $(KERNELDIR) M=$(PWD) clean 
