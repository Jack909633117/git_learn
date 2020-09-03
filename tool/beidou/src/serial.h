#ifndef __SERIAL__
#define __SERIAL__

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#define SERIAL_BUFF_LEN             10240   //给10K的大小，保证数据完整性，当输出10Hz频率时数据量很大
#define SERIAL_SEND_BUFF_LEN        SERIAL_BUFF_LEN
#define SERIAL_RECIVE_BUFF_LEN      SERIAL_BUFF_LEN
#define PAGE_SIZE                   4096

typedef struct{
    bool run_flag;             //发送标志，有数据时置位
    unsigned int head;          //接收数据位置偏移
    unsigned int tail;          //发送数据位置偏移
    unsigned char buff[SERIAL_SEND_BUFF_LEN];
    unsigned char reading;/*1为在读,0为非在读状态，相对于发送DMA来说为读*/
    unsigned char writing;/*1为在向databuf中写数据，在写则不可读*/
    bool disable;
}Serial_Send_Type;
typedef struct{
    bool one_packet_flag;       //一包数据接收完成标志
    bool run_flag;             //接收标志，有数据时置位
    unsigned int head;          //接收数据位置偏移
    unsigned int tail;          //读取数据位置偏移
    unsigned char buff[SERIAL_RECIVE_BUFF_LEN];
    unsigned char reading;/*1为在读,0为非在读状态，相对于发送DMA来说为读*/
    unsigned char writing;/*1为在向databuf中写数据，在写则不可读*/
    bool disable;
}Serial_Recive_Type;
typedef struct{
    pthread_attr_t *thd_attr;//调整线程优先级使用
    struct sched_param sch_param;//调整线程优先级使用

    int fd_serial;
    Serial_Send_Type serial_send;
    Serial_Recive_Type serial_recv;
}Serial_Info_Type;

int serial_thread_init(Serial_Info_Type *serial_info);
uint32_t serial_data_read(Serial_Info_Type *serial_info, char *buf,uint32_t *tail);
uint32_t serial_read_data(Serial_Info_Type *serial_info, uint8_t *buf, uint32_t buf_size, uint32_t *tail);
void serial_data_load(Serial_Info_Type *serial_info, const char *buf, unsigned int len);
int serial_open(char *dev);
int serial_init(int nFd, int buad);
void serial_destroy(Serial_Info_Type *serial_info);
#endif /* __SERIAL__ */
