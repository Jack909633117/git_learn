
# 使用方法
* make
* ./beidou  -dev /dev/ttyAMA0 -b 115200 

# RDSS短报文模块协议
* 串口非同步传送，参数定义如下: 
* 传输速率：19200bit/s（默认），可根据用户机具体情况设置其它速率； 
* 1 bit 开始位； 
* 8 bit 数据位； 
* 1 bit 停止位；  
* 无校验 。 
* 接口数据传输基本格式如下： 
* 指令 / 内容 长度 用户地址 信息内容 校验和 
* “指令”或“内容”传输时以 ASCII 码表示，每个 ASCII 码为一个字节； 
* “长度”表示从 “指令或内容”起始符“$”开始到“校验和”（含校验和）为
* 止的数据总字节数； 
* “用户地址”为与外设相连的用户机 ID 号，长度为 3 字节，其中有效位为低
* 21bit，高 3bit 填“0”； 
* “校验和”是指从“指令或内容”起始符“$”起到“校验和”前一字节，按字
* 节异或的结果； 
* “信息内容”用二进制原码表示，各参数项按格式要求的长度填充，不满长度要
* 求时，高位补“0”。信息按整字节传输，多字节信息先传高位字节，后传低位字节；
* 对于有符号参数，第 1 位符号位统一规定为“0”表示“+”，“1”表示“-”，其
* 后位数为参数值，用原码表示。

## 更详细的解析见北斗4.0协议

### IC检测:$ICJC
* TX，如：
* 24 49 43 4A 43 00 0C 00 00 00 00 2B 
* RX，如：
* 24 49 43 58 58 00 16 02 AD F7 00 00 00 0B 06 00 3C 03 00 00 00 52

### 系统自检（信号检测）:$XTZJ
* TX，如：
* 24 58 54 5A 4A 00 0D 00 00 00 00 00 35 
* RX，如：
* 

### 串口输出（设置波特率）:$CKSC
* TX，如115200：
* 24 43 4B 53 43 00 0C 00 00 00 07 37 
* RX，如：
* 

### 定位申请（单次定位）:$DWSQ
* TX，如：
* 24 44 57 53 51 00 16 00 00 00 04 00 00 00 00 00 00 00 00 00 00 27 
* RX，如：
* 

### 时间输出:$SJSC
* TX，如输出频度60s：
* 24 53 4A 53 43 00 0D 00 00 00 00 3C 1C  
* RX，如：
* 

### 通信申请:$TXSQ
* TX，如收信方123456,报文类型混发（北斗模块通信测试[混发模式]-!@#$%^&*()~1234567890_ABCDEFGHIJKLMNOPQRSTUVWXYZ.）：
* 24 54 58 53 51 00 5F 00 00 00 46 01 E2 40 02 68 00 A4 B1 B1 B6 B7 C4 A3 BF * E9 CD A8 D0 C5 B2 E2 CA D4 5B BB EC B7 A2 C4 A3 CA BD 5D 2D 21 40 23 24 25 * 5E 26 2A 28 29 7E 31 32 33 34 35 36 37 38 39 30 5F 41 42 43 44 45 46 47 48 * 49 4A 4B 4C 4D 4E 4F 50 51 52 53 54 55 56 57 58 59 5A 2E 2C  
* RX，如：
* 

### 功率检测(注：该指令不推荐使用，相应功能在“系统自检”（$XTZJ）中实现):$GLJC
* TX，如：
* 
* RX，如：
* 

### 航线设置 ($HXSZ)
* TX，如：
* 
* RX，如：
* 

### 定时申请（$DSSQ）
* TX，如：
* 
* RX，如：
* 

###  版本读取（$BBDQ）
* TX，如：
* 
* RX，如：
* 

###  版本信息（$BBXX）
* TX，如：
* 
* RX，如：
* 