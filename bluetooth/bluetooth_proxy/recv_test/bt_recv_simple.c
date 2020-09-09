#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include "shm.h"

share_mem_type share_mem = {
    .shmkey = BLUEZ_APP_SHMKEY,
    .shm_type = SHM_SIZE_10K_TYPE,
};

void printfhex(unsigned char *buf, unsigned int len)
{
    int i;
    printf("\r\nmain process recv data len is :%d\r\n", len);
    for(i=0;i<len;i++){
        printf("%02x ", buf[i]);
    }
    printf("\r\n");
}

int bluetooth_proxy_init(void)
{
    shm_creat(&share_mem);

    return 0;
}

int main (void)
{
//  bluetooth_proxy_init();

    int recv_len = 0;
    unsigned char recv_buff[SHARE_MEM_BUF_LEN] = {0};
    share_mem_type *bluez_share_mem = &share_mem;

    shm_creat(bluez_share_mem);
    printf("shm init complete!shmid is:%d\n", bluez_share_mem->shmid);
    while(1) {
        usleep(50000);//50ms检测一次状态
        recv_len = shm_read_data(bluez_share_mem, recv_buff, SHM_DATA_A_TYPE);
        if(recv_len > 0) {
            printf("recv_buff is:%s\n", recv_buff);
        }
    }
}
