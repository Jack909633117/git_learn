#include "beidou.h"
#include <signal.h>
#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>

#define INSTRUCTION_SIZE 5
#define PACKET_LEN_SIZE 2
#define USER_ADDR_SIZE 3
#define CHECKSUM_SIZE 1

#define IPUC (INSTRUCTION_SIZE + PACKET_LEN_SIZE + USER_ADDR_SIZE + CHECKSUM_SIZE)

//通信申请
#define TXSQ_INFO_FIRM_SIZE 7 //即(1 个信息类别 + 3 个用户地址 + 2个电文长度 + 1个应答字节)
#define TXSQ_FIRM_SIZE (IPUC + TXSQ_INFO_FIRM_SIZE)

//IC检测
#define ICJC_INFO_FIRM_SIZE 1 //即帧号，占一个字节
#define ICJC_FIRM_SIZE (IPUC + ICJC_INFO_FIRM_SIZE)

//系统自检
#define XTZJ_INFO_FIRM_SIZE 2 //即自检频度，占二个字节
#define XTZJ_FIRM_SIZE (IPUC + XTZJ_INFO_FIRM_SIZE)

//IC信息
#define ICXX_INFO_FIRM_SIZE 11 //即(1个帧号+3个通播ID+1个用户特征+2个服务频度+1个通信等级+1个加密标志+2个下属用户总数)
#define ICXX_FIRM_SIZE (IPUC + ICXX_INFO_FIRM_SIZE)

#define TXXX_INFO_FIRM_SIZE 9 //即(1个信息类别+3个发信方地址+2个发信时间+2个电文长度+1个CRC标志
#define TXXX_FIRM_SIZE (IPUC + TXXX_INFO_FIRM_SIZE)
#define TXXX_MAX_SIZE (TXXX_FIRM_SIZE + MAX_PAYLOAD_LEN) //TXXX由固定长度和净长度构成

#define FKXX_INFO_FIRM_SIZE 5 //即(1个反馈标志+4个附加信息)
#define FKXX_FIRM_SIZE (IPUC + FKXX_INFO_FIRM_SIZE)

#define ZJXX_INFO_FRIM_SIZE 10 //即(1个IC卡状态+1个硬件状态+1个电池电量+1个入站状态+6个功率状态)
#define ZJXX_FIRM_SIZE (IPUC + ZJXX_INFO_FRIM_SIZE)

#define RX_BD_MAX_DATA_SIZE TXXX_MAX_SIZE

#define TXSQ_PAYLOAD_CHINESE 0x44
#define TXSQ_PAYLOAD_BCD 0x46

#define beidou_dbg(x, arg...) debug_printf("[BD]" x, ##arg)
#define beidou_err(x, arg...) debug_printf("[BD]" x, ##arg)
#define beidou_print(x, arg...) debug_printf("[BD]" x, ##arg)
#define BEIDOU_USART_DEV "/dev/ttyAMA1"
#define BEIDOU_USART_DEFAULT_BUAD 115200

typedef struct beidou_struct
{
    // beidou模块串口信息
    Serial_Info_Type bd_serial;
    /* 串口节点 */
    uint8_t serial_dev[64];
    /* 波特率 */
    uint32_t baud_rate;
    // 缓冲区：主要是用于发送打包
    uint8_t buff[1024];
    // 运行状态:0/关闭  1/开启
    uint8_t run;
    // 打印信息状态:0/关闭  >0/开启
    uint8_t debug;
    // 波特率匹配情况状态:0/未匹配  1/成功
    uint8_t state;
    // 控制类型
    uint8_t ctrl;
    // 北斗数据信息
    msg_t msg;
} beidou_t;

beidou_t *bd = NULL;

#define BEIDOU_VERSION "V1.0 - 20200721@slj"

static void signal_kill_handle(int sig)
{
    printf("--->> beidou: Exit now <<---\r\n");

    if (SIGINT == sig ||
        SIGTSTP == sig ||
        SIGTERM == sig)
    {
        if (bd)
        {
            serial_destroy(&bd->bd_serial);
            beidou_free_msg(&bd->msg);
            free(bd);
            bd = NULL;
        }
    }

    exit(0);
}

static void debug_printf(const char *fmt, ...)
{
    if (!bd)
        return;

    if (bd->debug)
    {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
    }
}

/*******************************************************************************
 * 名称: beidou_memcpy
 * 功能: 复制内存
 * 形参: *des:目的地址
 *      *src:源地址
 *       n:需要复制的内存长度(字节为单位)
 * 返回: 无
 * 说明: 拷贝小内存时不调用库函数
 ******************************************************************************/
void beidou_memcpy(void *des, void *src, uint32_t n)
{
    uint8_t *xdes = des;
    uint8_t *xsrc = src;
    while (n--)
        *xdes++ = *xsrc++;
}

/*******************************************************************************
 * 名称: beidou_printfhex
 * 功能: 把数组数据转换为十六进制格式打印
 * 形参: buf_name：数组名称
        buf：数组指针
        len：数据长度
 * 返回: 无
 * 说明: 无
 ******************************************************************************/
static void beidou_printfhex(int8_t *buf_name, uint8_t *buf, uint32_t len)
{
    int i;
    if (bd->debug)
    {
        beidou_dbg("%s len is :%d\r\n", buf_name, len);
        for (i = 0; i < len; i++)
        {
            printf("%02X ", buf[i]);
        }
        printf("\r\n");
    }
}

static void beidou_sleepms(int ms)
{
    struct timespec ts;

    if (ms <= 0)
        return;
    ts.tv_sec = (time_t)(ms / 1000);
    ts.tv_nsec = (long)(ms % 1000 * 1000000);
    nanosleep(&ts, NULL);
}

//
// bool BCDtoStr(char *str, uint8_t *BCD, int BCD_length)
// {
//     if (BCD == 0 || BCD_length == 0)
//         return false;
//     int i, j;
//     for (i = 0, j = 0; i < BCD_length; i++, j += 2)
//     {
//         str[j] = (BCD[i] >> 4) > 9 ? (BCD[i] >> 4) - 10 + 'A' : (BCD[i] >> 4) + '0';
//         str[j + 1] = (BCD[i] & 0x0F) > 9 ? (BCD[i] & 0x0F) - 10 + 'A' : (BCD[i] & 0x0F) + '0';
//     }
//     str[j] = '\0';
//     return true;
// }

// bool StrtoBCD(char *str, uint8_t *BCD, int *BCD_length)
// {
//     if (str == 0)
//         return false;
//     int tmp = strlen(str);
//     tmp -= tmp % 2;
//     if (tmp == 0)
//         return false;
//     int i, j;

//     for (i = 0; i < tmp; i++)
//     {
//         if (str == 0 ||
//             !(str[i] >= '0' && str[i] <= '9' ||
//               str[i] >= 'a' && str[i] <= 'f' ||
//               str[i] >= 'A' && str[i] <= 'F'))
//             return false;
//     }

//     for (i = 0, j = 0; i < tmp / 2; i++, j += 2)
//     {
//         str[j] > '9' ? (str[j] > 'F' ? BCD[i] = str[j] - 'a' + 10 : BCD[i] = str[j] - 'A' + 10) : BCD[i] = str[j] - '0';
//         str[j + 1] > '9' ? (str[j + 1] > 'F' ? BCD[i] = (BCD[i] << 4) + str[j + 1] - 'a' + 10 : BCD[i] = (BCD[i] << 4) + str[j + 1] - 'A' + 10) : BCD[i] = (BCD[i] << 4) + str[j + 1] - '0';
//     }
//     if (BCD_length)
//         *BCD_length = tmp / 2;
//     return true;
// }

// int32_t TD_ASC2BCD(uint8_t *bcd, const int8_t *asc, int32_t len, int32_t fmt)
// {
//     int32_t i, odd;
//     int8_t ch;

//     odd = len & 0x01;
//     if (odd && !fmt)
//     {
//         *bcd++ = (*asc++) & 0x0F;
//     }
//     len >>= 1;
//     for (i = 0; i < len; i++)
//     {
//         ch = (*asc++) << 4;
//         ch |= (*asc++) & '\x0F';
//         *bcd++ = ch;
//     }

//     if (odd && fmt)
//     {
//         *bcd = (*asc) << 4;
//     }
//     return (i + odd);
// }

static void help(int8_t *argv0)
{
    printf(
        "\n"
        "Usage: %s [option]\n"
        "\n"
        "Opition:\n"
        "  -d   : 显示打印信息[1,2,...255]\n"
        "  -dev : 串口节点\n"
        "  -b   : 波特率\n"
        "  -? --help : 显示帮助\n"
        "\n"
        "软件版本: %s\n"
        "\n"
        "Example:\n"
        "  %s  -dev /dev/ttyAMA0 -b 115200 -d 2\n"
        "\n",
        argv0, BEIDOU_VERSION, argv0);
}


void print_icxx(icxx_t *icxx)
{
    if (bd->debug >= 2)
    {
        printf("    ========= ICXX包-打印开始=========\r\n");
        printf("    ICXX包总长度:%d\r\n", icxx->packet_len);
        printf("    ICXX包用户地址:0x%02x%02x%02x\r\n", icxx->user_addr[0], icxx->user_addr[1], icxx->user_addr[2]);
        printf("    ICXX信息内容-帧号:%d\r\n", icxx->icxx_info.frame_id);
        printf("    ICXX信息内容-通播ID:0x%02x%02x%02x\r\n", icxx->icxx_info.broadcast_id[0], icxx->icxx_info.broadcast_id[1], icxx->icxx_info.broadcast_id[2]);
        printf("    ICXX信息内容-用户特征:%d\r\n", icxx->icxx_info.user_feature);
        printf("    ICXX信息内容-服务频度:%d秒\r\n", icxx->icxx_info.service_frequency);
        printf("    ICXX信息内容-通信级别:%d\r\n", icxx->icxx_info.comm_level);
        printf("    ICXX信息内容-加密标志:%d\r\n", icxx->icxx_info.encryption_flag);
        printf("    ICXX信息内容-用户数目:%d\r\n", icxx->icxx_info.user_num);
        printf("    ICXX包校验和:0x%02x\r\n", icxx->checksum);
        printf("    ========= ICXX包-打印结束=========\r\n");
    }
}

void print_zjxx(zjxx_t *zjxx)
{
    if (bd->debug >= 2)
    {
        printf("    ========= ZJXX包-打印开始=========\r\n");
        printf("    ZJXX包总长度:%d\r\n", zjxx->packet_len);
        printf("    ZJXX包用户地址:0x%02x%02x%02x\r\n", zjxx->user_addr[0], zjxx->user_addr[1], zjxx->user_addr[2]);
        printf("    ZJXX信息内容-IC卡状态:0x%02x\r\n", zjxx->zjxx_info.ic_status);
        printf("    ZJXX信息内容-硬件状态:0x%02x\r\n", zjxx->zjxx_info.hw_status);
        printf("    ZJXX信息内容-电池电量:0x%02x\r\n", zjxx->zjxx_info.battery_quantity);
        printf("    ZJXX信息内容-入站状态:0x%02x\r\n", zjxx->zjxx_info.in_station_status);
        printf("    ZJXX信息内容-功率状态:%d-%d-%d-%d-%d-%d\r\n", zjxx->zjxx_info.power_status[0], zjxx->zjxx_info.power_status[1],
               zjxx->zjxx_info.power_status[2], zjxx->zjxx_info.power_status[3], zjxx->zjxx_info.power_status[4], zjxx->zjxx_info.power_status[5]);
        printf("    ZJXX包校验和:0x%02x\r\n", zjxx->checksum);
        printf("    ========= ZJXX包-打印结束=========\r\n");
    }
}
void print_fkxx(fkxx_t *fkxx)
{
    if (bd->debug >= 2)
    {
        printf("    ========= FKXX包-打印开始=========\r\n");
        printf("    FKXX包总长度:%d\r\n", fkxx->packet_len);
        printf("    FKXX包用户地址:0x%02x%02x%02x\r\n", fkxx->user_addr[0], fkxx->user_addr[1], fkxx->user_addr[2]);
        printf("    FKXX信息内容-反馈标志:0x%02x\r\n", fkxx->fkxx_info.fk_flag);
        printf("    FKXX信息内容-附加信息:0x%02x-0x%02x-0x%02x-0x%02x\r\n", fkxx->fkxx_info.extra_info[0], fkxx->fkxx_info.extra_info[1], fkxx->fkxx_info.extra_info[2], fkxx->fkxx_info.extra_info[3]);
        printf("    FKXX包校验和:0x%02x\r\n", fkxx->checksum);
        printf("    ========= FKXX包-打印结束=========\r\n");
    }
}

void print_txxx(txxx_t *txxx)
{
    unsigned int i;

    if (bd->debug >= 2)
    {
        printf("    ========= TXXX包-打印开始=========\r\n");
        printf("    TXXX包总长度:%d\r\n", txxx->packet_len);
        printf("    TXXX包用户地址:0x%02x%02x%02x\r\n", txxx->user_addr[0], txxx->user_addr[1], txxx->user_addr[2]);
        printf("    TXXX包信息内容-发送方地址:0x%02x%02x%02x\r\n", txxx->txxx_info.src_user_addr[0], txxx->txxx_info.src_user_addr[1], txxx->txxx_info.src_user_addr[2]);
        printf("    TXXX包信息内容-发送时间:%02d时%02d分\r\n", txxx->txxx_info.send_time.hour, txxx->txxx_info.send_time.minute);
        printf("    TXXX包信息内容-电文长度:%d字节\r\n", txxx->txxx_info.payload_len / 8);
        printf("    TXXX包信息内容-电文内容:");
        for (i = 0; i < (txxx->txxx_info.payload_len / 8); ++i)
        {
            printf("%02x ", txxx->txxx_info.payload[i]);
        }
        printf("\r\n");
        printf("    TXXX包信息内容-CRC:0x%02x\r\n", txxx->txxx_info.crc);
        printf("    TXXX包校验和:0x%02x\r\n", txxx->checksum);
        printf("    ========= TXXX包-打印结束=========\r\n");
    }
}

/*******************************************************************************
 * 名称: xor_checksum
 * 功能: 异或校验和计算
 * 形参: *buf : 数据
 *       len : 数据长度
 * 返回: 检验和
 * 说明: 异或校验和计算
 ******************************************************************************/
static uint8_t xor_checksum(uint8_t *buf, uint32_t len)
{
    uint32_t i;
    uint8_t checksum = 0;

    if(!buf) return 0;

    for (i = 0; i < len; ++i)
    {
        checksum ^= *(buf++);
    }

    return checksum;
}

/*******************************************************************************
 * 名称: beidou_match_msg_id
 * 功能: 匹配消息ID
 * 形参: *buff : 数据
 * 返回: 配对ID
 * 说明: 匹配消息ID
 ******************************************************************************/
uint32_t beidou_match_msg_id(const int8_t *buff)
{
    if(!buff) return 0;

    if (!strncmp(buff, "DWXX", 4))
        return DWXX_MSG;
    if (!strncmp(buff, "TXXX", 4))
        return TXXX_MSG;
    if (!strncmp(buff, "ICXX", 4))
        return ICXX_MSG;
    if (!strncmp(buff, "ZJXX", 4))
        return ZJXX_MSG;
    if (!strncmp(buff, "SJXX", 4))
        return SJXX_MSG;
    if (!strncmp(buff, "BBXX", 4))
        return BBXX_MSG;
    if (!strncmp(buff, "FKXX", 4))
        return FKXX_MSG;

    return NONE_MSG;
}

/*******************************************************************************
 * 名称: sys_beidou
 * 功能: 北斗协议头检测
 * 形参: *buff : 数据
 *      data : 导入字节
 * 返回: 配对ID
 * 说明: 北斗协议头检测
 ******************************************************************************/
static int sys_beidou(uint8_t *buff, uint8_t data)
{
    if(!buff) return 0;
    /* 数据移动 */
    buff[0] = buff[1];
    buff[1] = buff[2];
    buff[2] = buff[3];
    buff[3] = buff[4];
    buff[4] = data;
    /* 匹配数据头 */
    if (buff[0] == '$' && 'A' <= buff[1] <= 'Z')
        return beidou_match_msg_id(buff + 1);
    else
        return NONE_MSG;
}

/*******************************************************************************
 * 名称: beidou_input_msg_data
 * 功能: 导入解码数据
 * 形参: *msg : msg_t消息结构
 *      data : 导入字节
 * 返回: 解码结果
 * 说明: 导入解码数据
 ******************************************************************************/
int beidou_input_msg_data(msg_t *msg, uint8_t data)
{
    if(!msg) return 0;
    // beidou_dbg("beidou_input_msg_data: data=0x%02x\n", data);

    /* 未检测到头数据 */
    if (msg->nbyte == 0)
    {
        msg->type = sys_beidou(msg->buff, data);
        if (msg->type == NONE_MSG)
            return 0;
        msg->nbyte = 5;
        return 0;
    }

    /* 插入数据 */
    msg->buff[msg->nbyte++] = data;

    /* 数据越界判断保护 */
    if (msg->nbyte >= MAX_BEIDOU_RAW)
    {
        msg->nbyte = 0;
        msg->type = NONE_MSG;
        return 0;
    }

    /* 数据长度提取判断 */
    if (msg->nbyte == 7)
    {
        msg->len = msg->buff[5] << 8 | msg->buff[6];

        beidou_dbg("msg len : len=%d\r\n", msg->len);
        if (msg->len > MAX_BEIDOU_RAW)
            /* 数据长度大于buff长度，分包接收 */
            beidou_dbg("length large buff len : len=%d\r\n", msg->len);
    }
    if (msg->nbyte < 7 || msg->nbyte < msg->len)
        return 0;

    msg->nbyte = 0;

    /* decode beidou message */
    return decode_beidou(msg);
}

/*******************************************************************************
 * 名称: beidou_decode_icxx
 * 功能: 解码ICXX语句
 * 形参: *msg : msg_t消息结构
 * 返回: 解码结果
 * 说明: IC信息
 ******************************************************************************/
int beidou_decode_icxx(msg_t *msg)
{
    if(!msg) return 0;

    icxx_t *icxx = msg->icxx;
    /* 描述符 帧号 */
    strncpy(icxx->instruction, msg->buff, 5);
    /* 包长度 */
    icxx->packet_len = msg->len;
    /* 用户地址 */
    beidou_memcpy(icxx->user_addr, msg->buff + 7, 3);
    /* 帧号 */
    icxx->icxx_info.frame_id = msg->buff[10];
    /* 通播ID */
    beidou_memcpy(icxx->icxx_info.broadcast_id, msg->buff + 11, 3);
    /* 用户特征 */
    icxx->icxx_info.user_feature = msg->buff[14];
    /* 服务频度 */
    icxx->icxx_info.service_frequency = msg->buff[15] << 8 | msg->buff[16];
    /* 通信等级 */
    icxx->icxx_info.comm_level = msg->buff[17];
    /* 加密标志 */
    icxx->icxx_info.encryption_flag = msg->buff[18];
    /* 下属用户数 */
    icxx->icxx_info.user_num = msg->buff[19] << 8 | msg->buff[20];
    /* 校验 */
    icxx->checksum = msg->buff[21];

    print_icxx(icxx);

    return 1;
}

/*******************************************************************************
 * 名称: beidou_decode_zjxx
 * 功能: 解码ZJXX语句
 * 形参: *msg : msg_t消息结构
 * 返回: 解码结果
 * 说明: 自检信息
 ******************************************************************************/
int beidou_decode_zjxx(msg_t *msg)
{
    if(!msg) return 0;

    zjxx_t *zjxx = msg->zjxx;
    /* 描述符 帧号 */
    strncpy(zjxx->instruction, msg->buff, 5);
    /* 包长度 */
    zjxx->packet_len = msg->len;
    /* 用户地址 */
    beidou_memcpy(zjxx->user_addr, msg->buff + 7, 3);
    /* ic 状态 */
    zjxx->zjxx_info.ic_status = msg->buff[10];
    /* 硬件状态 */
    zjxx->zjxx_info.hw_status = msg->buff[11];
    /* 电池容量 */
    zjxx->zjxx_info.battery_quantity = msg->buff[12];
    /* 入站状态 */
    zjxx->zjxx_info.in_station_status = msg->buff[13];
    /* 功率状况 */
    beidou_memcpy(zjxx->zjxx_info.power_status, &msg->buff[14], 6);
    /* 校验 */
    zjxx->checksum = msg->buff[20];

    print_zjxx(zjxx);

    return 1;
}

/*******************************************************************************
 * 名称: beidou_decode_txxx
 * 功能: 解码TXXX语句
 * 形参: *msg : msg_t消息结构
 * 返回: 解码结果
 * 说明: 通信信息
 ******************************************************************************/
int beidou_decode_txxx(msg_t *msg)
{
    unsigned int payload_len;

    if(!msg) return 0;

    txxx_t *txxx = msg->txxx;
    /* 描述符 帧号 */
    strncpy(txxx->instruction, msg->buff, 5);
    /* 包长度 */
    txxx->packet_len = msg->len;
    /* 用户地址 */
    beidou_memcpy(txxx->user_addr, msg->buff + 7, 3);
    /* 信息-信息类别 */
    beidou_memcpy(&txxx->txxx_info.txxx_info_type, &msg->buff[10], 1);
    /* 信息-发送方地址 */
    beidou_memcpy(txxx->txxx_info.src_user_addr, &msg->buff[11], 3);
    /* 信息-发送时间 */
    txxx->txxx_info.send_time.hour = msg->buff[14];
    txxx->txxx_info.send_time.minute = msg->buff[15];
    /* 信息-电文长度 */
    txxx->txxx_info.payload_len = msg->buff[16] << 8 | msg->buff[17];
    payload_len = txxx->txxx_info.payload_len / 8;
    /* 信息-电文内容 */
    beidou_memcpy(txxx->txxx_info.payload, &msg->buff[18], payload_len);
    /* 信息-CRC */
    txxx->txxx_info.crc = msg->buff[18 + payload_len];
    /* 校验和 */
    txxx->checksum = msg->buff[18 + payload_len + 1];

    print_txxx(txxx);

    return 1;
}

/*******************************************************************************
 * 名称: beidou_decode_fkxx
 * 功能: 解码FKXX语句
 * 形参: *msg : msg_t消息结构
 * 返回: 解码结果
 * 说明: 反馈信息
 ******************************************************************************/
int beidou_decode_fkxx(msg_t *msg)
{
    if(!msg) return 0;

    fkxx_t *fkxx = msg->fkxx;
    /* 描述符 帧号 */
    strncpy(fkxx->instruction, msg->buff, 5);
    /* 包长度 */
    fkxx->packet_len = msg->len;
    /* 用户地址 */
    beidou_memcpy(fkxx->user_addr, msg->buff + 7, 3);
    /* 反馈标志 */
    fkxx->fkxx_info.fk_flag = msg->buff[10];
    /* 附加信息 */
    beidou_memcpy(fkxx->fkxx_info.extra_info, msg->buff + 11, 4);
    /* 校验 */
    fkxx->checksum = msg->buff[15];

    print_fkxx(fkxx);

    return 1;
}

int beidou_decode_bbxx(msg_t *msg)
{
    return 0;
}
int beidou_decode_dwxx(msg_t *msg)
{
    return 0;
}
int beidou_decode_sjxx(msg_t *msg)
{
    return 0;
}

/*******************************************************************************
 * 名称: decode_beidou
 * 功能: 北斗解码
 * 形参: *msg : msg_t消息结构
 * 返回: 解码结果
 * 说明: 无
 ******************************************************************************/
int decode_beidou(msg_t *msg)
{
    if(!msg) return 0;

    beidou_printfhex(msg->buff + 1, msg->buff, msg->len);

    switch (msg->type)
    {
    case ICXX_MSG:
        return beidou_decode_icxx(msg);
    case ZJXX_MSG:
        return beidou_decode_zjxx(msg);
    case TXXX_MSG:
        return beidou_decode_txxx(msg);
    case FKXX_MSG:
        return beidou_decode_fkxx(msg);
    case BBXX_MSG:
        return beidou_decode_bbxx(msg);
    case DWXX_MSG:
        return beidou_decode_dwxx(msg);
    case SJXX_MSG:
        return beidou_decode_sjxx(msg);
    default:
        break;
    }
    return 0;
}

// 1、IC检测协议内容:
//   icjc:24 49 43 4A 43 00 0C 00 00 00 00 2B
//   icxx:24 49 43 58 58 00 16 02 AD F7 00 00 00 0B 06 00 3C 03 00 00 00 52
/*******************************************************************************
 * 名称: beidou_encode_icjc
 * 功能: 系统自检ICJC协议编码
 * 形参: buff     : 编码数据存放地址
 * 返回: 长度
 * 说明: 无
 ******************************************************************************/
uint32_t beidou_encode_icjc(int8_t *buff)
{
    if(!buff) return 0;

    buff[0] = '$';
    buff[1] = 'I';
    buff[2] = 'C';
    buff[3] = 'J';
    buff[4] = 'C';

    buff[5] = ICJC_FIRM_SIZE / 256; //先传高位
    buff[6] = ICJC_FIRM_SIZE % 256; //再传低位

    buff[7] = 0x00;
    buff[8] = 0x00;
    buff[9] = 0x00;

    buff[10] = 0x00;

    buff[11] = xor_checksum(buff, (ICJC_FIRM_SIZE - 1));

    return 12;
}

// 1、系统自检XTZJ协议内容:
//     xtzj:24 58 54 5A 4A 00 0D 02 AD FB 00 00 61
//     zjxx:24 5a 4a 58 58 00 15 02 AD FB 01 00 64 02 00 00 03 00 02 00 13
/*******************************************************************************
 * 名称: beidou_encode_xtzj
 * 功能: 系统自检XTZJ协议编码
 * 形参: buff     : 编码数据存放地址
 *      user_addr: 用户地址
 *      frea     : 自检频度
 * 返回: 长度
 * 说明: 无
 ******************************************************************************/
int32_t beidou_encode_xtzj(int8_t *buff, uint64_t user_addr, uint32_t freq)
{
    if(!buff) return 0;

    buff[0] = '$';
    buff[1] = 'X';
    buff[2] = 'T';
    buff[3] = 'Z';
    buff[4] = 'J';

    buff[5] = XTZJ_FIRM_SIZE / 256; //先传高位
    buff[6] = XTZJ_FIRM_SIZE % 256; //再传低位

    buff[7] = user_addr / 65536;
    buff[8] = (user_addr % 65536) / 256;
    buff[9] = (user_addr % 65536) % 256;

    buff[10] = freq / 256;
    buff[11] = freq % 256;

    buff[12] = xor_checksum(buff, XTZJ_FIRM_SIZE - 1);

    return 13;
}

// 1、结构体不宜管理可变长度的数据协议,如通讯申请协议
// 2、发送长度为6个字节("我爱你")，发送方式为中文，协议内容:
//     txsq:24 54 58 53 51 00 18 02 ad f7 44 02 ad f7 00 30 00 ce d2 b0 ae c4 e3 63
//     txxx:24 54 58 58 58 00 1a 02 ad f7 40 02 ad f7 00 00 00 30 ce d2 b0 ae c4 e3 00 67
/*******************************************************************************
 * 名称: beidou_encode_txsq                        
 * 功能: 通信申请TXSQ协议编码
 * 形参: buff     : 编码数据存放地址
 *      user_addr: 用户地址
 *      dst_addr : 目的地址
 *      fmt      : 报文格式：0/汉字 1/代码，混合
 *      payload  : 报文载体
 *      payload_len: 报文长度
 * 返回: 长度
 * 说明: 无
 ******************************************************************************/
int32_t beidou_encode_txsq(uint8_t *buff, uint64_t src_user,
                           uint64_t dst_user, uint8_t fmt,
                           uint8_t *payload, uint32_t payload_len)
{
    if(!buff && !payload) return 0;
    /* 1、通信申请指令初始化 */
    buff[0] = '$';
    buff[1] = 'T';
    buff[2] = 'X';
    buff[3] = 'S';
    buff[4] = 'Q';

    /* 2、包长度，先传高位，再传低位 */
    buff[5] = (TXSQ_FIRM_SIZE + payload_len) / 256;
    buff[6] = (TXSQ_FIRM_SIZE + payload_len) % 256;

    /* 3、源用户地址,先传高位，再传低位 */
    buff[7] = src_user / 65536;
    buff[8] = (src_user % 65536) / 256;
    buff[9] = (src_user % 65536) % 256;

    /* 4.1、信息-信息类别 */
    if (fmt == 0)                        //汉字
        buff[10] = TXSQ_PAYLOAD_CHINESE; //0b01000100;
    else                                 //代码/混发
        buff[10] = TXSQ_PAYLOAD_BCD;     //0b01000110;

    /* 4.2、信息-目的用户地址 */
    buff[11] = dst_user / 65536;
    buff[12] = (dst_user % 65536) / 256;
    buff[13] = (dst_user % 65536) % 256;

    /* 4.3、信息-电文净荷长度-单位是bit */
    buff[14] = (payload_len * 8) / 256;
    buff[15] = (payload_len * 8) % 256;

    /* 4.4、信息-是否应答 */
    buff[16] = 0;
    // buff[17] = 0XA4; //测试软件多了一个字节？，具体定义没有
    /* 4.5、信息-电文内容 */
    beidou_memcpy(&buff[17], payload, payload_len);

    /* 5、校验和 */
    buff[TXSQ_FIRM_SIZE + payload_len - 1] = xor_checksum(buff, (TXSQ_FIRM_SIZE + payload_len - 1));

    return TXSQ_FIRM_SIZE + payload_len + 1;
}

/*******************************************************************************
 * 名称: beidou_throwOut_thread
 * 功能: 抛出线层，退出时自动释放线程资源
 * 形参: *data: 数据传递
 *      *callback: 线程函数
 * 返回: 无
 * 说明: 创建线程
 ******************************************************************************/
void beidou_throwOut_thread(void *data, void *callback)
{
    pthread_t th;
    pthread_attr_t attr;
    //
    //attr init
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED); //禁用线程同步, 线程运行结束后自动释放
    //抛出线程
    pthread_create(&th, &attr, callback, (void *)data);
    //attr destroy
    pthread_attr_destroy(&attr);
}

/*******************************************************************************
 * 名称: beidou_module_init
 * 功能: 北斗模块初始化
 * 形参: bd: beidou_t结构体指针
 * 返回: 0：失败 1：成功
 * 说明: 无
 ******************************************************************************/
uint8_t beidou_module_init(beidou_t *bd)
{
    if (!bd)
        return NULL;

    bd->bd_serial.fd_serial = open(bd->serial_dev, O_RDWR | O_NOCTTY, 0777);
    if (bd->bd_serial.fd_serial <= 0)
    {
        beidou_dbg("Open serial device %s error! \r\n", bd->serial_dev);
        return NULL;
    }
    serial_init(bd->bd_serial.fd_serial, bd->baud_rate);

    serial_thread_init(&bd->bd_serial);

    bd->run = 1;

    return 1;
}

/*******************************************************************************
 * 名称: beidou_recv_data_decode
 * 功能: 接收数据解码
 * 形参: *buf: 数据
 *       len: 数据长度
 * 返回: 处理结果0:未处理 1：处理成功
 * 说明: 接收数据解码
 ******************************************************************************/
int32_t beidou_recv_data_decode(int8_t *buf, uint32_t len)
{
    int i, ret = 0;

    if(!buf) return 0;

    for (i = 0; i < len; i++)
    {
        ret = beidou_input_msg_data(&bd->msg, buf[i]);
    }

    return ret;
}

/*******************************************************************************
 * 名称: beidou_on_data_recv
 * 功能: 串口数据接收
 * 形参: *bd: beidou_t结构
 *      *recv_callbalk: 数据处理回调
 * 返回: len：数据长度
 * 说明: 串口数据接收
 ******************************************************************************/
int32_t beidou_on_data_recv(beidou_t *bd, int32_t (*recv_callbalk)(int8_t *buf, uint32_t len))
{
    uint8_t buff[1024] = {0};
    uint32_t len = 0;
    static uint32_t bd_tail = 0;

    if(!bd) return 0;

    len = serial_read_data(&bd->bd_serial, buff, sizeof(buff), &bd_tail);
    if (len > 0 && !recv_callbalk)
        recv_callbalk(buff, len);

    return len;
}

/*******************************************************************************
 * 名称: beidou_ctrl_uage
 * 功能: 控制指令说明
 * 形参: 无
 * 返回: 无
 * 说明: 控制指令说明
 ******************************************************************************/
void beidou_ctrl_uage(void)
{
    printf(
        "Please input the control command:\r\n"
        "1   : 通信申请\r\n"
        "2   : IC检测\r\n"
        "3   : 系统自检\r\n"
        "0   : 退出\r\n");
}

/*******************************************************************************
 * 名称: beidou_ctrl_proc_thrd
 * 功能: 控制指令处理线程
 * 形参: *bd: beidou_t结构
 * 返回: 无
 * 说明: 释放消息结构内存
 ******************************************************************************/
int32_t beidou_ctrl_proc_thrd(beidou_t *bd)
{

    int8_t cmd;
    int ret = 0;
    uint32_t payload_len = 0;
    uint8_t payload[MAX_PAYLOAD_LEN];

    if(!bd) return 0;

    beidou_ctrl_uage();

    while (1)
    {
        if (scanf("%c", &cmd))
            getchar();
        printf("===>> c:%c\r\n", cmd);

        switch (cmd)
        {
        case '1':
            printf("\r\n    输入发送内容(代码):");
            scanf("%s", payload);
            payload_len = strlen((char const *)payload);
            ret = beidou_encode_txsq(bd->buff,
                                     0x04dd2c, 0x04dd2c, 1,
                                     payload, payload_len);
            if (ret > 0)
            {
                beidou_printfhex("TXSQ", bd->buff, ret);
                serial_data_load(&bd->bd_serial, bd->buff, ret);
            }
            break;
        case '2':
            ret = beidou_encode_icjc(bd->buff);
            if (ret > 0)
            {
                beidou_printfhex("ICJC", bd->buff, ret);
                serial_data_load(&bd->bd_serial, bd->buff, ret);
            }
            break;
        case '3':
            ret = beidou_encode_xtzj(bd->buff, 0x04dd2c, 60);
            if (ret > 0)
            {
                beidou_printfhex("XTZJ", bd->buff, ret);
                serial_data_load(&bd->bd_serial, bd->buff, ret);
            }
            break;

        case '0':
            signal_kill_handle(0);
            break;

        default:
            beidou_ctrl_uage();
            break;
        }
        beidou_sleepms(100);
    }

    return 0;
}

/*******************************************************************************
 * 名称: beidou_free_msg
 * 功能: 释放消息结构内存
 * 形参: *msg: 消息结构
 * 返回: 无
 * 说明: 释放消息结构内存
 ******************************************************************************/
void beidou_free_msg(msg_t *msg)
{
    if(!msg) return 0;

    free(msg->bbxx);
    free(msg->dwxx);
    free(msg->icxx);
    free(msg->txxx);
    free(msg->zjxx);
    free(msg->sjxx);

    msg->bbxx = NULL;
    msg->dwxx = NULL;
    msg->icxx = NULL;
    msg->txxx = NULL;
    msg->zjxx = NULL;
    msg->sjxx = NULL;
}

/*******************************************************************************
 * 名称: beidou_init_msg
 * 功能: 初始化消息结构
 * 形参: *msg: 消息结构
 * 返回: 1/成功 0/失败
 * 说明: 申请结构内存
 ******************************************************************************/
int beidou_init_msg(msg_t *msg)
{
    if(!msg) return 0;

    if (!(msg->bbxx = (bbxx_t *)calloc(sizeof(bbxx_t), 1)) ||
        !(msg->dwxx = (dwxx_t *)calloc(sizeof(dwxx_t), 1)) ||
        !(msg->icxx = (icxx_t *)calloc(sizeof(icxx_t), 1)) ||
        !(msg->txxx = (txxx_t *)calloc(sizeof(txxx_t), 1)) ||
        !(msg->zjxx = (zjxx_t *)calloc(sizeof(zjxx_t), 1)) ||
        !(msg->sjxx = (sjxx_t *)calloc(sizeof(sjxx_t), 1)) ||
        !(msg->fkxx = (fkxx_t *)calloc(sizeof(fkxx_t), 1)))
    {
        beidou_dbg("beidou_init_msg: calloc err!");
        beidou_free_msg(msg);
        return 0;
    }
    msg->len = msg->nbyte = msg->type = 0;

    return 1;
}

/*******************************************************************************
 * 名称: beidou_init
 * 功能: 北斗应用初始化
 * 形参: bd: beidou_t结构体指针
 * 返回: 初始化指针
 * 说明: 申请结构内存
 ******************************************************************************/
void *beidou_init(beidou_t *bd)
{
    bd = (beidou_t *)calloc(sizeof(beidou_t), 1);

    if (!bd)
    {
        beidou_dbg("beidou_init: bd calloc err!");
        return NULL;
    }

    memset(bd, 0, sizeof(beidou_t));
    beidou_init_msg(&bd->msg);

    return bd;
}
/* ***************************** test ********************************* */
const int8_t icxx_buff[] = {
    0x24, 0x49, 0x43, 0x58, 0x58, 0x00, 0x16, 0x02, 0xAD, 0xF7, 0x00, 0x00,
    0x00, 0x0B, 0x06, 0x00, 0x3C, 0x03, 0x00, 0x00, 0x00, 0x52};
const int8_t zjxx_buff[] = {
    0x24, 0x5a, 0x4a, 0x58, 0x58, 0x00, 0x15, 0x02, 0xAD, 0xFB, 0x01, 0x00,
    0x64, 0x02, 0x00, 0x00, 0x03, 0x00, 0x02, 0x00, 0x13};
const int8_t txxx_buff[] = {
    0x24, 0x54, 0x58, 0x58, 0x58, 0x00, 0x1a, 0x02, 0xad, 0xf7, 0x40, 0x02, 0xad,
    0xf7, 0x00, 0x00, 0x00, 0x30, 0xce, 0xd2, 0xb0, 0xae, 0xc4, 0xe3, 0x00, 0x67};
const int8_t fkxx_buff[] = {
    0x24, 0x46, 0x4B, 0x58, 0x58, 0x00, 0x10, 0x04, 0xDD,
    0x2C, 0x02, 0x54, 0x58, 0x53, 0x51, 0xC0};

/*******************************************************************************
 * 名称: main
 * 功能: 程序主函数
 * 形参: argc: 参数个数
 *      argv： 具体参数
 * 返回: 0
 * 说明: 无
 ******************************************************************************/
int main(int32_t argc, int8_t *argv[])
{
    int i;

    bd = beidou_init(bd);

    strcpy(bd->serial_dev, BEIDOU_USART_DEV);
    bd->baud_rate = BEIDOU_USART_DEFAULT_BUAD;

    bd->debug = 0;

    for (i = 1; i < argc; i++)
    {
        if (!strncmp(argv[i], "-dev", 4) && i + 1 < argc)
        {
            strcpy(bd->serial_dev, argv[++i]);
        }
        else if (!strncmp(argv[i], "-b", 2) && i + 1 < argc)
        {
            sscanf(argv[i], "%d", &bd->baud_rate);
        }
        else if (!strncmp(argv[i], "-d", 2))
        {
            if (i + 1 < argc)
            {
                if (strncmp(argv[++i], "-", 1) != 0)
                {
                    sscanf(argv[i], "%d", &bd->debug);
                    continue;
                }
            }
            bd->debug = 1;
        }
        else if (!strncmp(argv[i], "-?", 2) || !strncmp(argv[i], "-help", 5))
        {
            help(argv[0]);
            return 0;
        }
    }
    printf("[beidou] serial_dev:%s baud rate:%d debug:%d \r\n",
           bd->serial_dev, bd->baud_rate, bd->debug);

    signal(SIGINT, signal_kill_handle);
    signal(SIGKILL, signal_kill_handle);

    // 模块初始化
    if (beidou_module_init(bd) != 1)
    {
        exit(0);
    }

    //命令控制
    beidou_throwOut_thread(bd, beidou_ctrl_proc_thrd);
    while (1)
    {
        beidou_on_data_recv(bd, beidou_recv_data_decode);
        beidou_sleepms(10);
        // beidou_recv_data_decode(icxx_buff, sizeof(icxx_buff));
        // beidou_recv_data_decode(zjxx_buff, sizeof(zjxx_buff));
        // beidou_recv_data_decode(txxx_buff, sizeof(txxx_buff));
        // beidou_recv_data_decode(fkxx_buff, sizeof(fkxx_buff));
        // beidou_sleepms(1000);
    }

    return 0;
}