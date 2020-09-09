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
share_mem_10k_type *bluez_shm_pointer;
int bluetooth_proxy_init(void)
{
    shm_creat(&share_mem);
    bluez_shm_pointer = (share_mem_10k_type *)share_mem.shm_pointer;
    return 0;
}

void printfhex(unsigned char *buf, unsigned int len)
{
    int i;
    printf("\r\nproxy send data len is :%d\r\n", len);
    for(i=0;i<len;i++){
        printf("%02x ", buf[i]);
    }
    printf("\r\n");
}

int bluetooth_proxy_write(char* dat, int len)
{
//  printfhex(dat, len);

    //python send data to main process share memery
    shm_write_data(&share_mem, dat, len, SHM_DATA_A_TYPE);
    return 0;
}

int bluetooth_proxy_read(char* dat)
{
//  memset(dat, 0, SHM_SIZE_10K_TYPE);
    return shm_read_data(&share_mem, dat, SHM_DATA_B_TYPE);
}

int bluetooth_status_set(int sta)
{
    if(bluez_shm_pointer)
    {
        // printf("bluez sta is:%d\n\n\n", sta);
        bluez_shm_pointer->dat[BT_CONNECT_IDEX] = sta;
    }
}