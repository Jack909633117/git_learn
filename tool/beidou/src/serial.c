
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/ioctl.h>

#include "serial.h"

/*
* serial define
*/
typedef struct
{
    unsigned int lable;
    unsigned int baudrate;
} SERIAL_BAUD_ST;

typedef struct
{
    unsigned int flags;
    unsigned int delay_rts_after_send; /* delay after send (milliseconds) */
} SERIAL_RS485_CONF_ST;

typedef struct
{
    char parity;
    unsigned int baud;
    unsigned int databits;
    unsigned int stopbits;
} SERIAL_ATTR_ST;

#define TIMEOUT 1   /* read operation timeout 1s = TIMEOUT/10 */
#define MIN_LEN 128 /* the min len datas */
#define DEV_NAME_LEN 11
#define SERIAL_ATTR_BAUD 115200
#define SERIAL_ATTR_DATABITS 8
#define SERIAL_ATTR_STOPBITS 1
#define SERIAL_ATTR_PARITY 'n'
#define SERIAL_MODE_NORMAL 0
#define SERIAL_MODE_485 1
#define DELAY_RTS_AFTER_SEND 1 /* 1ms */
#define SER_RS485_ENABLED 1    /* 485 enable */

#define PER_READ_BYTE 8
#define PER_WRITE_BYTE 8

static SERIAL_BAUD_ST g_attr_baud[] = {
    {921600, B921600},
    {460800, B460800},
    {230400, B230400},
    {115200, B115200},
    {57600, B57600},
    {38400, B38400},
    {19200, B19200},
    {9600, B9600},
    {4800, B4800},
    {2400, B2400},
    {1800, B1800},
    {1200, B1200},
};
static void serial_sleepms(int ms)
{
    struct timespec ts;
    if (ms <= 0)
        return;
    ts.tv_sec = (time_t)(ms / 1000);
    ts.tv_nsec = (long)(ms % 1000 * 1000000);
    nanosleep(&ts, NULL);
}
/*******************************************************************************
 * 名称: thrd_serial_send
 * 功能: 串口发送线程创建回调函数，负责串口数据的发送
 * 形参: arg：Serial_Info_Type结构体指针
 * 返回: 无
 * 说明: 数据是循环发送的，当tail达到SERIAL_SEND_BUFF_LEN时从0开始
 ******************************************************************************/
void *thrd_serial_send(void *arg)
{
    unsigned int data_len = 0;
    unsigned int head_bak = 0, i;
    unsigned int write_count = 0;
    unsigned int time_count = 0;
    bool data_falg = false; //数据标志，有收到数据置位
    Serial_Info_Type *serial_info;
    serial_info = (Serial_Info_Type *)arg;
    while (serial_info->serial_send.run_flag)
    {
        if (serial_info->serial_send.disable) // 禁用write
        {
            serial_sleepms(500);
            continue;
        }

        if (data_falg)
        {
            serial_sleepms(1); //有数据那就1ms延时
        }
        else
        {
            serial_sleepms(200); //没有数据那就200ms延时
        }

        if (serial_info->serial_send.writing == 0) //没有写的时候执行
        {
            if (serial_info->serial_send.head != serial_info->serial_send.tail)
            {
                data_falg = true;
                head_bak = serial_info->serial_send.head;
                if (head_bak > serial_info->serial_send.tail)
                {
                    data_len = head_bak - serial_info->serial_send.tail;
                    {
                        write_count = data_len / PAGE_SIZE;
                        for (i = 0; i < write_count; i++) //每次发送PAGE_SIZE个字节
                        {
                            if (write(serial_info->fd_serial, &serial_info->serial_send.buff[serial_info->serial_send.tail + i * PAGE_SIZE],
                                      PAGE_SIZE) < 0)
                            {
                                printf("Write serial error!\r\n");
                            }
                            tcdrain(serial_info->fd_serial); //等待输出完毕
                        }
                        if (write(serial_info->fd_serial, &serial_info->serial_send.buff[serial_info->serial_send.tail + i * PAGE_SIZE],
                                  data_len - i * PAGE_SIZE) < 0)
                        {
                            printf("Write serial error!\r\n");
                        }
                        tcdrain(serial_info->fd_serial); //等待输出完毕
                    }
                }
                else if (head_bak < serial_info->serial_send.tail)
                {
                    data_len = SERIAL_SEND_BUFF_LEN - serial_info->serial_send.tail;
                    write_count = data_len / PAGE_SIZE;
                    for (i = 0; i < write_count; i++) //每次发送PAGE_SIZE个字节
                    {
                        if (write(serial_info->fd_serial, &serial_info->serial_send.buff[serial_info->serial_send.tail + i * PAGE_SIZE],
                                  PAGE_SIZE) < 0)
                        {
                            printf("Write serial error!\r\n");
                        }
                        tcdrain(serial_info->fd_serial); //等待输出完毕
                    }
                    if (write(serial_info->fd_serial, &serial_info->serial_send.buff[serial_info->serial_send.tail + i * PAGE_SIZE],
                              data_len - i * PAGE_SIZE) < 0)
                    {
                        printf("Write serial error!\r\n");
                    }
                    tcdrain(serial_info->fd_serial); //等待输出完毕

                    data_len = head_bak;
                    write_count = data_len / PAGE_SIZE;
                    for (i = 0; i < write_count; i++) //每次发送PAGE_SIZE个字节
                    {
                        if (write(serial_info->fd_serial, &serial_info->serial_send.buff[serial_info->serial_send.tail + i * PAGE_SIZE],
                                  PAGE_SIZE) < 0)
                        {
                            printf("Write serial error!\r\n");
                        }
                        tcdrain(serial_info->fd_serial); //等待输出完毕
                    }
                    if (write(serial_info->fd_serial, &serial_info->serial_send.buff[serial_info->serial_send.tail + i * PAGE_SIZE],
                              data_len - i * PAGE_SIZE) < 0)
                    {
                        printf("Write serial error!\r\n");
                    }
                    tcdrain(serial_info->fd_serial); //等待输出完毕
                }
                serial_info->serial_send.tail = head_bak;
            }
            if (time_count++ > 20) //当运行20次后还是没有读到数据，那么设置data_falg标志为ｆａｌｓｅ
            {
                time_count = 0;
                data_falg = false;
            }
        }
    }
    pthread_exit(NULL); //退出线程
}
/*******************************************************************************
 * 名称: thrd_serial_recv
 * 功能: 串口接收线程创建回调函数，负责串口数据的接收
 * 形参: arg：Serial_Info_Type结构体指针
 * 返回: 无
 * 说明: 每次接收最大只有PER_READ_BYTE（8）个字节，数据分多次接收，read为阻塞方式
 ******************************************************************************/
void *thrd_serial_recv(void *arg)
{
    int nRet = 0;
    unsigned int one_packet_count = 0;
    unsigned int read_byte = 8;
    Serial_Info_Type *serial_info;
    serial_info = (Serial_Info_Type *)arg;
    while (serial_info->serial_recv.run_flag)
    {
        if (serial_info->serial_recv.disable) // 禁用read
        {
            serial_sleepms(500);
            continue;
        }
        else
            nRet = read(serial_info->fd_serial, &serial_info->serial_recv.buff[serial_info->serial_recv.head], read_byte);
        if (-1 == nRet)
        {
            perror("Read Data Error !\r\n");
            break;
        }
        serial_info->serial_recv.head += nRet;

        if (serial_info->serial_recv.head >= SERIAL_RECIVE_BUFF_LEN)
        {
            serial_info->serial_recv.head = serial_info->serial_recv.head - SERIAL_RECIVE_BUFF_LEN;
        }
        else if ((serial_info->serial_recv.head + PER_READ_BYTE) > SERIAL_RECIVE_BUFF_LEN)
        {
            read_byte = SERIAL_RECIVE_BUFF_LEN - serial_info->serial_recv.head;
        }
        else
        {
            read_byte = PER_READ_BYTE;
        }
        one_packet_count += nRet;
        if (nRet < PER_READ_BYTE)
        {
            serial_info->serial_recv.one_packet_flag = true;
            one_packet_count = 0;
        }
    }
    pthread_exit(NULL); //退出线程
}

/* 抛出线层，退出时自动释放线程资源 */
void _throwOut_thread(void *data, void *callback)
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
 * 名称: serial_thread_init
 * 功能: 串口线程创建初始化
 * 形参: serial_info：Serial_Info_Type结构体指针
 * 返回: 0：初始化成功；-1：初始化失败
 * 说明: 调用该函数即可完成串口的发送和接收两个线程的创建
 ******************************************************************************/
int serial_thread_init(Serial_Info_Type *serial_info)
{
    serial_info->serial_recv.head = 0;
    serial_info->serial_recv.tail = 0;
    serial_info->serial_recv.disable = false;
    serial_info->serial_recv.run_flag = true;
    _throwOut_thread(serial_info, thrd_serial_recv);
    serial_info->serial_send.head = 0;
    serial_info->serial_send.tail = 0;
    serial_info->serial_send.disable = false;
    serial_info->serial_send.run_flag = true;
    _throwOut_thread(serial_info, thrd_serial_send);
    return 0;
}
/*******************************************************************************
 * 名称: serial_data_read
 * 功能: 串口数据读取
 * 形参: gnss：gnss_type结构体指针
 * 返回: 无
 * 说明: 无
 ******************************************************************************/
uint32_t serial_data_read(Serial_Info_Type *serial_info, char *buf, uint32_t *tail)
{
    unsigned int buf_len = 0;
    unsigned int head_bak = 0;

    head_bak = serial_info->serial_recv.head; //先保存接收偏移，数据可能还在接收载入
    if (*tail < SERIAL_BUFF_LEN)              //做个保护判断，以防出现莫名问题，涉及数组比较危险
    {
        if (head_bak > *tail)
        {
            memcpy(buf,
                   &serial_info->serial_recv.buff[*tail],
                   head_bak - *tail);
            buf_len = head_bak - *tail;
        }
        else if (head_bak < *tail)
        {
            memcpy(buf,
                   &serial_info->serial_recv.buff[*tail],
                   SERIAL_RECIVE_BUFF_LEN - *tail);
            memcpy(&buf[SERIAL_RECIVE_BUFF_LEN - *tail],
                   &serial_info->serial_recv.buff[0], head_bak);
            buf_len = SERIAL_RECIVE_BUFF_LEN - *tail + head_bak;
            ;
        }
    }
    *tail = head_bak;
    return buf_len;
}
/*******************************************************************************
 * 名称: serial_data_read
 * 功能: 串口数据读取
 * 形参: 
 * 返回: 无
 * 说明: 无
 ******************************************************************************/
uint32_t serial_read_data(Serial_Info_Type *serial_info, uint8_t *buf, uint32_t buf_size, uint32_t *tail)
{
    uint32_t i = 0;
    uint32_t head_bak = 0;
    uint32_t rd_tail = *tail;

    head_bak = serial_info->serial_recv.head; //先保存接收偏移，数据可能还在接收载入
    while (rd_tail != head_bak && i < buf_size)
    {
        buf[i++] = serial_info->serial_recv.buff[rd_tail];
        rd_tail += 1;
        if (rd_tail >= SERIAL_BUFF_LEN)
            rd_tail = 0;
    }
    *tail = rd_tail;
    return i;
}
/*******************************************************************************
 * 名称: serial_data_load
 * 功能: 串口数据装载
 * 形参: serial_info：Serial_Info_Type结构体指针
        buf：待装载的数据指针
        len：装载的长度
 * 返回: 无
 * 说明: 无
 ******************************************************************************/
void serial_data_load(Serial_Info_Type *serial_info, const char *buf, unsigned int len)
{
    while (serial_info->serial_send.writing == 1) //等待前面的写入完成
    {
        serial_sleepms(1); //1ms延时
    }
    serial_info->serial_send.writing = 1;
    if (serial_info->serial_send.head + len < SERIAL_SEND_BUFF_LEN)
    {
        memcpy(&serial_info->serial_send.buff[serial_info->serial_send.head], buf, len);
        serial_info->serial_send.head += len;
    }
    else
    {
        memcpy(&serial_info->serial_send.buff[serial_info->serial_send.head], buf, SERIAL_SEND_BUFF_LEN - serial_info->serial_send.head);
        memcpy(&serial_info->serial_send.buff[0], &buf[SERIAL_SEND_BUFF_LEN - serial_info->serial_send.head], len - (SERIAL_SEND_BUFF_LEN - serial_info->serial_send.head));
        serial_info->serial_send.head = len - (SERIAL_SEND_BUFF_LEN - serial_info->serial_send.head);
    }
    serial_info->serial_send.writing = 0; //允许下一次数据的装载
}

static int attr_baud_set(int fd, unsigned int baud)
{
    int i;
    int ret = 0;
    struct termios option;

    /* get old serial attribute */
    memset(&option, 0, sizeof(option));
    if (0 != tcgetattr(fd, &option))
    {
        printf("tcgetattr failed.\n");
        return -1;
    }

    for (i = 0; i < sizeof(g_attr_baud) / sizeof(g_attr_baud[0]); i++)
    {
        if (baud == g_attr_baud[i].lable)
        {
            ret = tcflush(fd, TCIOFLUSH);
            if (0 != ret)
            {
                printf("tcflush failed.\n");
                break;
            }

            ret = cfsetispeed(&option, g_attr_baud[i].baudrate);
            if (0 != ret)
            {
                printf("cfsetispeed failed.\n");
                ret = -1;
                break;
            }

            ret = cfsetospeed(&option, g_attr_baud[i].baudrate);
            if (0 != ret)
            {
                printf("cfsetospeed failed.\n");
                ret = -1;
                break;
            }

            ret = tcsetattr(fd, TCSANOW, &option);
            if (0 != ret)
            {
                printf("tcsetattr failed.\n");
                ret = -1;
                break;
            }

            ret = tcflush(fd, TCIOFLUSH);
            if (0 != ret)
            {
                printf("tcflush failed.\n");
                break;
            }
        }
    }

    return ret;
}

static int attr_other_set(int fd, SERIAL_ATTR_ST *serial_attr)
{
    struct termios option;

    /* get old serial attribute */
    memset(&option, 0, sizeof(option));
    if (0 != tcgetattr(fd, &option))
    {
        printf("tcgetattr failed.\n");
        return -1;
    }

    option.c_iflag = CLOCAL | CREAD;

    /* set datas size */
    option.c_cflag &= ~CSIZE;
    option.c_iflag = 0;

    switch (serial_attr->databits)
    {
    case 7:
        option.c_cflag |= CS7;
        break;

    case 8:
        option.c_cflag |= CS8;
        break;

    default:
        printf("invalid argument, unsupport datas size.\n");
        return -1;
    }

    /* set parity */
    switch (serial_attr->parity)
    {
    case 'n':
    case 'N':
        option.c_cflag &= ~PARENB;
        option.c_iflag &= ~INPCK;
        break;

    case 'o':
    case 'O':
        option.c_cflag |= (PARODD | PARENB);
        option.c_iflag |= INPCK;
        break;

    case 'e':
    case 'E':
        option.c_cflag |= PARENB;
        option.c_cflag &= ~PARODD;
        option.c_iflag |= INPCK;
        break;

    case 's':
    case 'S':
        option.c_cflag &= ~PARENB;
        option.c_cflag &= ~CSTOPB;
        break;

    default:
        printf("invalid argument, unsupport parity type.\n");
        return -1;
    }

    /* set stop bits  */
    switch (serial_attr->stopbits)
    {
    case 1:
        option.c_cflag &= ~CSTOPB;
        break;

    case 2:
        option.c_cflag |= CSTOPB;
        break;

    default:
        printf("invalid argument, unsupport stop bits.\n");
        return -1;
    }

    option.c_oflag = 0;
    option.c_lflag = 0;
    option.c_cc[VTIME] = TIMEOUT;
    option.c_cc[VMIN] = MIN_LEN;

    if (0 != tcflush(fd, TCIFLUSH))
    {
        printf("tcflush failed.\n");
        return -1;
    }

    if (0 != tcsetattr(fd, TCSANOW, &option))
    {
        printf("tcsetattr failed.\n");
        return -1;
    }

    return 0;
}
static int attr_set(int fd, SERIAL_ATTR_ST *serial_attr)
{
    int ret = 0;

    if (NULL == serial_attr)
    {
        printf("invalid argument.\n");
        return -1;
    }

    if (0 == ret)
    {
        ret = attr_baud_set(fd, serial_attr->baud);
        if (0 == ret)
        {
            ret = attr_other_set(fd, serial_attr);
        }
    }

    return ret;
}

/*******************************************************************************
 * 名称: serial_init
 * 功能: 串口参数设置初始化
 * 形参: nFd：串口设备文件描述符
        buad：串口的波特率
 * 返回: nFd：设置成功；-1：设置失败
 * 说明: 无
 ******************************************************************************/
int serial_init(int fd, int buad)
{
    int ret;
    SERIAL_ATTR_ST serial_attr;

    memset(&serial_attr, 0, sizeof(serial_attr));
    serial_attr.baud = buad;
    serial_attr.databits = SERIAL_ATTR_DATABITS;
    serial_attr.stopbits = SERIAL_ATTR_STOPBITS;
    serial_attr.parity = SERIAL_ATTR_PARITY;
    printf("====serial_init\r\n");

    ret = attr_set(fd, &serial_attr);
    serial_sleepms(20); //延时10ms确保服务创建完成
    return ret;
}

void serial_destroy(Serial_Info_Type *serial_info)
{
    if (serial_info == NULL)
        return;
    //
    close(serial_info->fd_serial);

    serial_info->fd_serial = 0;
}