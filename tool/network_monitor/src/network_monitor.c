#if 1
#include <pcap.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <pthread.h>
#include <netinet/in.h>
#include "network_monitor.h"
#include "redis_common.h"

#define MONITOR_VERSION "V1.0 - 20200630"

const char netList[][32] = {"usb0", "wlan1"};

const char wlan1_mode_config_path[] = "/usr/local/fw6118/ap/mode.conf";

typedef struct
{
    uint64_t total_cnt;
    uint64_t tcp_cnt;
    uint64_t udp_cnt;
    int net_speed;
    time_t timestamp;
} check_t;
/* 监控结构体 */
typedef struct
{
    pcap_t *handle;       /* 打开的设备 */
    char *dev;            /* 监控的设备名称 */
    char filter_exp[128]; /* 监控表达式 */
    void *rc;             /* redis属性结构体 */
    uint8_t debug;        /* 调试使能 */
    uint8_t mode;         /* 运行模式 0：正常运行 1：自定义运行 */
    uint8_t run;          /* 0:监控未运行，1：监控运行 */
    check_t check;        /* 数据统计结构 */
} network_monitor_t;

static network_monitor_t monitor = {0};

static void debug_printf(const char *fmt, ...)
{
    if (monitor.debug)
    {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
    }
}
/*
 *以太网帧的首部
 */
typedef struct ethernet
{
    uint8_t host1[6];
    uint8_t host2[6];
    uint16_t type;
} Ethernet;

/* IPv4 首部 */
typedef struct ip_header
{
    uint8_t ver_ihl;         // 版本 (4 bits) + 首部长度 (4 bits)
    uint8_t tos;             // 服务类型(Type of service)
    uint16_t tlen;           // 总长(Total length)
    uint16_t identification; // 标识(Identification)
    uint16_t flags_fo;       // 标志位(Flags) (3 bits) + 段偏移量(Fragment offset) (13 bits)
    uint8_t ttl;             // 存活时间(Time to live)
    uint8_t proto;           // 协议(Protocol)
    uint16_t crc;            // 首部校验和(Header checksum)
    int saddr;               // 源地址(Source address)
    int daddr;               // 目的地址(Destination address)
} Ip_header;

/* UDP 首部*/
typedef struct udp_header
{
    uint16_t sport; // 源端口(Source port)
    uint16_t dport; // 目的端口(Destination port)
    uint16_t len;   // UDP数据包长度(Datagram length),the minimum is 8
    uint16_t crc;   // 校验和(Checksum)
} Udp_header;

/*TCP首部*/
typedef struct tcp_header
{
    uint16_t sport;
    uint16_t dPort;
    unsigned int uiSequNum;
    unsigned int uiAcknowledgeNum;
    uint16_t sHeaderLenAndFlag;
    uint16_t sWindowSize;
    uint16_t sCheckSum;
    uint16_t surgentPointer;
} Tcp_header;

/* 限制速度获取 */
float monitor_get_limitSpeed(float ratecut, float speed, int speed_order)
{
    float errSpeed = 0;
    static float _speed[3] = {0};

    _speed[0] = _speed[1];
    _speed[1] = _speed[2];
    _speed[2] = speed;
    if (speed_order < 3)
        return errSpeed;
    errSpeed = (_speed[0] + _speed[1] + _speed[2]) / 3.0 - ratecut;
    if (errSpeed >= 0)
        return 0;
    else
        return errSpeed;
}

/* 网络监控线程 */
void *network_monitor_thread(void *data)
{
    time_t temp;
    uint64_t total_bytes;
    uint64_t tcp_bytes;
    uint64_t udp_bytes;
    char buf[32];
    static uint64_t speed_order = 0;
    float speed, tcp_speed, udp_speed;

    monitor.run = 1;
    total_bytes = monitor.check.total_cnt;
    tcp_bytes = monitor.check.tcp_cnt;
    udp_bytes = monitor.check.udp_cnt;

    while (monitor.run)
    {
        time(&temp);

        if (temp - monitor.check.timestamp > 0)
        {
            speed_order += 1;
            speed = (monitor.check.total_cnt - total_bytes) / 1024.0;
            tcp_speed = (monitor.check.tcp_cnt - tcp_bytes) / 1024.0;
            udp_speed = (monitor.check.udp_cnt - udp_bytes) / 1024.0;
            if (!monitor.mode)
            {
                memset(buf, 0, sizeof(buf));
                snprintf(buf, sizeof(buf), "%.2f", speed);
                redis_setStr(monitor.rc, "rtmp_speed", buf);
                debug_printf("[RTMP MONITOR]: ratecut:%d Kb/s,rtmp speed:%0.2f Kb/s err:%0.2f\r\n",
                             redis_getInt(monitor.rc, "minor_ratecut", -1) / 8, speed,
                             monitor_get_limitSpeed(redis_getInt(monitor.rc, "minor_ratecut", -1) / 8.0, speed, speed_order));
            }
            else
            {
                debug_printf("[NETWORK]==>表达式:[%s]的网速：%.2f Kb/s \r\n", monitor.filter_exp, speed);
            }
            total_bytes = monitor.check.total_cnt;
            tcp_bytes = monitor.check.tcp_cnt;
            udp_bytes = monitor.check.udp_cnt;
            monitor.check.timestamp = temp;
            monitor.check.net_speed = speed;
        }
        usleep(10000);
    }
}

/* 数据监控回调 */
void got_packet(uint8_t *argv, const struct pcap_pkthdr *header, const uint8_t *packet)
{
    int caplen = (int)header->caplen;
    Ethernet *ethernet = (Ethernet *)(packet);
    Ip_header *ip = (Ip_header *)(packet + sizeof(Ethernet));

    if (ip->proto == (uint8_t)IPPROTO_TCP)
    {
        monitor.check.tcp_cnt += caplen;
    }
    else if (ip->proto == (uint8_t)IPPROTO_UDP)
    {
        monitor.check.udp_cnt += caplen;
    }
    monitor.check.total_cnt += caplen;
}

/* 1:网络已启动 其它：未运行 */
int check_monitor_net_run(void)
{
    int net_sta = -1;
    int ret = 0;

    if (strstr(monitor.dev, "wlan1"))
    {
        ret = redis_getInt(monitor.rc, "wlan1Status", -1);
        if (ret == 2)
            net_sta = 1;
    }
    else if (strstr(monitor.dev, "usb0"))
    {
        ret = redis_getInt(monitor.rc, "signal4g", -1);
        if (ret > 0)
            net_sta = 1;
    }
    return net_sta;
}

/* 启动rtmp监控 */
void rtmp_monitor_start(void)
{
    char errbuf[PCAP_ERRBUF_SIZE] = {0};
    struct bpf_program filter;
    bpf_u_int32 mask; /*net mask*/
    int id = 0;

    monitor.handle = pcap_open_live(monitor.dev, 65535, 1, 0, errbuf);

    if (!monitor.handle)
    {
        debug_printf("open device failed: %s", errbuf);
    }
    else
    {
        monitor.run = 1;
        //编译过滤器
        pcap_compile(monitor.handle, &filter, monitor.filter_exp, 0, mask);

        //配置过滤器
        pcap_setfilter(monitor.handle, &filter);

        network_throwOut_thread(NULL, network_monitor_thread);
        //-1表示循环抓包,阻塞
        pcap_loop(monitor.handle, -1, got_packet, (uint8_t *)&id);
    }
}

/* 停止rtmp监控 */
void rtmp_monitor_stop(void)
{
    monitor.run = 0;
    pcap_breakloop(monitor.handle);
    usleep(10000);
    pcap_close(monitor.handle);
}

/* rtmp推流检测线程 */
void *rtmp_network_monitor_thread(void *data)
{
    char rtmp_host[32] = {0};
    int rtmp_port = -1;
    int rtmp_flag = 0;

    while (1)
    { /* 等待监控网络启动 */
        if (check_monitor_net_run() == 1)
            break;

        usleep(1000000);
    }
    /* rtmp检测 */
    while (1)
    {
        rtmp_flag = redis_getInt(monitor.rc, "rtmp_flag", -1);
        if (rtmp_flag && !monitor.run)
        {
            redis_getStr(monitor.rc, "rtmp_ip", rtmp_host);
            rtmp_port = redis_getInt(monitor.rc, "rtmp_port", -1);
            if (rtmp_flag == 1 && strlen(rtmp_host) > 6 && rtmp_port > 0)
            {
                /* 构建表达式 */
                snprintf(monitor.filter_exp, sizeof(monitor.filter_exp), "dst %s and port %d",
                         rtmp_host, rtmp_port);
                

                debug_printf("===>>[NET MONITER]: debug/%d, dev/%s, exp/%s, mode/%d\r\n",
                             monitor.debug, monitor.dev, monitor.filter_exp, monitor.mode);
                //
                network_throwOut_thread(NULL, rtmp_monitor_start);
            }
        }
        else if (!rtmp_flag && monitor.run)
        {
            rtmp_monitor_stop();
            redis_setInt(monitor.rc, "rtmp_speed", 0);
            redis_setInt(monitor.rc, "err_speed", 0);
        }
        /* 延时1s */
        usleep(1000000);
    }
}

/* 用户网络监控线程 */
void *user_network_monitor_thread(void *data)
{
    char errbuf[PCAP_ERRBUF_SIZE] = {0};
    struct bpf_program filter;
    bpf_u_int32 mask; /*net mask*/
    int id = 0;

    debug_printf("===>>[NET MONITER]: debug/%d, dev/%s, exp/%s, mode/%d\r\n",
                 monitor.debug, monitor.dev, monitor.filter_exp, monitor.mode);
    //
    /* 获得用于捕获网络数据包的数据包捕获描述字 */
    /*  pcap_t *pcap_open_live(char *device, int snaplen,int promisc, int to_ms, char *ebuf)
        device：网络接口的名字，为第一步获取的网络接口字符串（pcap_lookupdev() 的返回值 ），也可人为指定，如“eth0”。
        snaplen：捕获数据包的长度，长度不能大于 65535 个字节。
        promise：“1” 代表混杂模式，其它非混杂模式。什么为混杂模式，请看《原始套接字编程》。
        to_ms：指定需要等待的毫秒数，超过这个数值后，获取数据包的函数就会立即返回（这个函数不会阻塞，后面的抓包函数才会阻塞）。
            0 表示一直等待直到有数据包到来。
        ebuf：存储错误信息。 
    */
    monitor.handle = pcap_open_live(monitor.dev, 65535, 1, 0, errbuf);
    if (!monitor.handle)
    {
        debug_printf("open device failed: %s", errbuf);
    }

    //编译过滤器
    pcap_compile(monitor.handle, &filter, monitor.filter_exp, 0, mask);

    //编译过滤器
    pcap_setfilter(monitor.handle, &filter);

    network_throwOut_thread(NULL, network_monitor_thread);
    //-1表示循环抓包
    pcap_loop(monitor.handle, -1, got_packet, (uint8_t *)&id);
    //
    pcap_close(monitor.handle);
}

/* 抛出线层，退出时自动释放线程资源 */
void network_throwOut_thread(void *data, void *callback)
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

/* wlan1模式检测 返回：1/AP 0/STA */
static int wlan1_mode_check(char *check_mode_path)
{
    char cfg_buf[128] = {0};
    int file_fd = 0;
    int ret = -1;

    if (access(check_mode_path, F_OK) != 0)
        return 0;
    file_fd = open(check_mode_path, O_RDWR, 0777);
    if (file_fd < 0)
    {
        return -1;
    }
    else
    {
        debug_printf(" %s open success!\n", check_mode_path);
    }
    if (read(file_fd, cfg_buf, sizeof(cfg_buf)) < 0)
    {
        debug_printf(" %s  read failed!\n", check_mode_path);
        return -1;
    }
    debug_printf("mode.conf is:%s\n", cfg_buf);
    if (strstr(cfg_buf, "AP"))
        return 1;
    else
        return 0;
}

/* 帮助 */
void help(char *argv0)
{
    printf(
        "\n"
        "Usage: %s [option]\n"
        "\n"
        "Opition:\n"
        "  -dev : 监控设备[wlan0,eth1,...]\n"
        "  -d   : 显示打印信息[0,1,...]\n"
        "  -exp : 过滤表达式\n"
        "       : 输入(type),比如：`host foo'，`net 128.3'，`port 20'\n"
        "       : 方向(dir),比如:'src foo'，'dst net 128.3'，`src or dst port ftp-data'\n"
        "       : 协议(proto),可能的协议有：ether，fddi，tr，ip，ip6，arp，rarp，decnet，tcp和udp。\n"
        "       : 比如：`ether src foo'，`arp net 128.3'，`tcp port 21'\n"
        "  -? --help : 显示帮助\n"
        "\n"
        "软件版本: %s\n"
        "\n"
        "Example:\n"
        "  %s  -dev wlan1 -exp host 192.168.43.27 -d\n"
        "  %s  -dev wlan1 -exp dst 49.4.7.254 and port 1936 -d\n"
        "\n",
        argv0, MONITOR_VERSION, argv0, argv0);
}

/* 应用主函数 */
int main(int argc, char *argv[])
{
    int i = 0;
    int wlan1Mode = -1;
    char *p = NULL;

    time(&monitor.check.timestamp);

    for (i = 1; i < argc; i++)
    {
        if (!strncmp(argv[i], "-dev", 4) && i + 1 < argc)
        {
            monitor.mode = 1;
            monitor.dev = argv[++i];
        }
        else if (!strncmp(argv[i], "-d", 2))
        {
            monitor.debug = 1;
        }
        else if (!strncmp(argv[i], "-?", 2) || !strncmp(argv[i], "-help", 5))
        {
            help(argv[0]);
            return 0;
        }
        else if (!strncmp(argv[i], "-exp", 4) && i + 1 < argc)
        {
            monitor.mode = 1;
            p = monitor.filter_exp;
            for (i = i + 1; i < argc; i++)
            {
                if (!strncmp(argv[i], "-", 1))
                {
                    *--p = '\0';
                    i--;
                    break;
                }
                strcpy(p, argv[i]);
                p += strlen(p);
                *p++ = ' ';
            }
        }
    }

    if (!monitor.mode)
    {
        monitor.rc = redis_connect("127.0.0.1", 6379);
        if (!monitor.rc)
        {
            debug_printf("Net monitor redis connect fail !");
            return -1;
        }
        else
        {
            /* 清空属性 */
            redis_setInt(monitor.rc, "rtmp_speed", 0);
        }
        wlan1Mode = wlan1_mode_check(wlan1_mode_config_path);
        if (wlan1Mode == 1)
            monitor.dev = netList[0];
        else
            monitor.dev = netList[1];
    }

    if (!monitor.mode)
    {
        network_throwOut_thread(NULL, rtmp_network_monitor_thread);
    }
    else
    {
        network_throwOut_thread(NULL, user_network_monitor_thread);
    }
    while (1)
    {
        usleep(1000000);
    }
    return 0;
}
#endif