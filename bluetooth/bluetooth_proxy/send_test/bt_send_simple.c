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
//
//  bluez_share_mem = (share_mem_type *)calloc(1, share_mem_type);
//  bluez_share_mem->shmkey = SHM_SIZE_10K_TYPE;
//  bluez_share_mem->shm_type = BLUEZ_APP_SHMKEY;

    shm_creat(bluez_share_mem);
//  bluez_share_mem->shmid = shmget(1010, sizeof(share_mem_10k_type), 0666 | IPC_CREAT);//其他进程只有读的权限
//  //将共享内存连接到当前进程的地址空间
//  bluez_share_mem->shm_pointer = shmat(bluez_share_mem->shmid, (void*)0, 0);
//  if(bluez_share_mem->shm_pointer == (void*)-1)
//  {
//      printf("shmat failed\n");
//  }
//  printf("Memory attached at %x\n", (int)bluez_share_mem->shm_pointer);
    printf("shm init complete!shmid is:%d\n", bluez_share_mem->shmid);
    while(1) {
        usleep(1000000);//50ms检测一次状态
        shm_write_data(bluez_share_mem, "123456789\r\n", strlen("123456789\r\n"), SHM_DATA_B_TYPE);
//      if(recv_len > 0) {
//          printf("recv_buff is:%s\n", recv_buff);
//      }
//      printf("send_buff is\n");
    }
}
