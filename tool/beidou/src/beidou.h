
#ifndef __BEIDOU_H
#define __BEIDOU_H
#include "serial.h"
#include <stdint.h>

#define MAX_PAYLOAD_LEN 210 //即(1680/8)
#define MAX_BEIDOU_RAW 512

/* 定位信息 */
typedef struct dwxx_struct
{
    uint32_t todo;
} dwxx_t;

/* 通信信息 */
typedef struct txxx_info_type_struct
{
    uint8_t packet_comm : 2;
    uint8_t transfer_format : 1;
    uint8_t ack : 1;
    uint8_t comm_way : 1;
    uint8_t has_key : 1;
    uint8_t rest : 2;
} txxx_info_type_t;

typedef struct send_time_struct
{
    uint8_t hour;
    uint8_t minute;
} send_time_t;

typedef struct txxx_info_struct
{
    txxx_info_type_t txxx_info_type;
    uint8_t src_user_addr[3];
    send_time_t send_time;
    uint32_t payload_len;
    uint8_t payload[MAX_PAYLOAD_LEN];
    uint8_t crc;
} txxx_info_t;

typedef struct txxx_struct
{
    uint8_t instruction[5];
    uint32_t packet_len; //解析结构体时以整形数据表示其长度
    uint8_t user_addr[3];
    txxx_info_t txxx_info;
    uint8_t checksum;
} txxx_t;

/* IC信息 */
typedef struct icxx_info_struct
{
    uint8_t frame_id;
    uint8_t broadcast_id[3];
    uint8_t user_feature;
    uint32_t service_frequency;
    uint8_t comm_level;
    uint8_t encryption_flag;
    uint32_t user_num;
}icxx_info_t;

typedef struct icxx_struct
{
    uint8_t instruction[5];
    uint32_t packet_len;
    uint8_t user_addr[3];
    icxx_info_t icxx_info;
    uint8_t checksum;
} icxx_t;

/* 自检信息 */
typedef struct zjxx_info_struct
{
    uint8_t ic_status;
    uint8_t hw_status;
    uint8_t battery_quantity;
    uint8_t in_station_status;
    uint8_t power_status[6];
}zjxx_info_t;

typedef struct zjxx_struct
{
    uint8_t instruction[5];
    uint32_t packet_len;
    uint8_t user_addr[3];
    zjxx_info_t zjxx_info;
    uint8_t checksum;
}zjxx_t;

/* 时间信息 */
typedef struct sjxx_struct
{
    uint8_t instruction[5];
    uint32_t packet_len;
    uint8_t user_addr[3];
    uint8_t sjxx_info[128]; /* 年月日时分秒，年为 16bit，其余均为 8bit */
    uint8_t checksum;
}sjxx_t;

/* 版本信息 */
typedef struct bbxx_struct
{
    uint8_t instruction[5];
    uint32_t packet_len;
    uint8_t user_addr[3];
    uint8_t bbxx_info[128]; /* 可见的字符串，用 ASCII 逗号分成若干段 */
    uint8_t checksum;
}bbxx_t;

/* 反馈信息 */
typedef struct fkxx_info_struct
{
    uint8_t fk_flag;
    uint8_t extra_info[4];
}fkxx_info_t;

typedef struct fkxx_struct
{
    uint8_t instruction[5];
    uint32_t packet_len;
    uint8_t user_addr[3];
    fkxx_info_t fkxx_info;
    uint8_t checksum;
}fkxx_t;

typedef struct
{
    uint16_t nbyte;
    uint16_t len;
    uint16_t type;
    uint8_t buff[MAX_BEIDOU_RAW];
    dwxx_t *dwxx;   /* 定位信息 */
    icxx_t *icxx;   /* IC 信息 */
    txxx_t *txxx;   /* 通信信息 */
    zjxx_t *zjxx;   /* 自检信息 */
    sjxx_t *sjxx;   /* 时间信息 */
    bbxx_t *bbxx;   /* 版本信息 */
    fkxx_t *fkxx;   /* 反馈信息 */
} msg_t;


/* 消息类型 */
typedef enum
{
    NONE_MSG = 0,
    DWXX_MSG, /* 定位信息 */
    TXXX_MSG, /* 通信信息 */
    ICXX_MSG, /* IC 信息 */
    ZJXX_MSG, /* 自检信息 */
    SJXX_MSG, /* 时间信息 */
    BBXX_MSG, /* 版本信息 */
    FKXX_MSG, /* 反馈信息  */
} MSG_TYPE;

#endif /* __BEIDOU_H */
