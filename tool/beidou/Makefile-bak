
#CROSS_COMPILE :=arm-himix200-linux-

#
GCC=$(CROSS_COMPILE)gcc
GXX=$(CROSS_COMPILE)g++

#当前工程的根目录路径
ROOT_DIR=$(shell pwd)

#.o文件存放路径, 方便清理
OBJ_DIR=$(ROOT_DIR)/obj

# include
INC += -I$(ROOT_DIR)/src

# 
CFLAG += -lpthread


#把所有 src/*.c 转换为 obj/*.o 并添加到obj
obj +=${patsubst %.c,$(OBJ_DIR)/%.o,${notdir ${wildcard $(ROOT_DIR)/src/*.c}}}

#编译所用引用到的.c为.o文件
%.o:../src/%.c
	$(GCC) -O3 -Wall -c $< $(INC) $(CFLAG)  -o $@

beidou:$(obj)
	$(GCC) -O3 -Wall -o beidou $(obj) $(INC) $(CFLAG)

clean:
	@rm -rf $(ROOT_DIR)/beidou  $(OBJ_DIR)/*.o 


