
CROSS_COMPILE :=arm-himix200-linux-

#
GCC=$(CROSS_COMPILE)gcc
GXX=$(CROSS_COMPILE)g++

#当前工程的根目录路径
ROOT_DIR=$(shell pwd)

#.o文件存放路径, 方便清理
OBJ_DIR=$(ROOT_DIR)/obj

# include
INC += -I$(ROOT_DIR)/include
INC += -I$(ROOT_DIR)/hiredis/include/hiredis

#
LIBS += -L$(ROOT_DIR)/hiredis/lib

# 
CFLAG += -lpthread -lhiredis

# ----- libpcap -----
INC2 += -I$(ROOT_DIR)/libpcap_himix200/include
INC2 += -I$(ROOT_DIR)/libpcap_himix200/include/pcap
LIBS2 += -L$(ROOT_DIR)/libpcap_himix200/lib
CFLAG2 += -lpcap

#把所有 src/*.c 转换为 obj/*.o 并添加到obj
obj +=${patsubst %.c,$(OBJ_DIR)/%.o,${notdir ${wildcard $(ROOT_DIR)/src/*.c}}}

#编译所用引用到的.c为.o文件
%.o:../src/%.c
	$(GCC) -O3 -Wall -c $< $(INC) $(LIBS) $(CFLAG) $(INC2) $(LIBS2) $(CFLAG2)  -o $@

network_monitor:$(obj)
	$(GCC) -O3 -Wall -o network_monitor $(obj) $(INC) $(LIBS) $(CFLAG) $(INC2) $(LIBS2) $(CFLAG2) 

clean:
	@rm -rf $(ROOT_DIR)/network_monitor  $(OBJ_DIR)/*.o 


