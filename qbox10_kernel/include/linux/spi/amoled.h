
#ifndef AMOLED_H
#define AMOLED_H

#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/fs.h>
#include <linux/time.h>
#include <linux/suspend.h>
#include <linux/cdev.h>
#include <linux/slab.h> //引用kmalloc  kfree
#include <mach/gpio.h>
#include <asm/uaccess.h>
#include <asm/io.h>
//
#include <linux/gpio.h> //gpio 控制
#include <linux/of_gpio.h>
//
#include <linux/delay.h>
#include <linux/time.h>
//
#include <linux/device.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/compat.h>
#include <linux/spi/spi.h>
#include <linux/types.h>

#include <linux/fb.h>
#include <linux/dma-mapping.h>

//具体宏在 linux/spi/spi.h 中找
#define SPI_MODE_MASK 0
// #define SPI_MODE_MASK (SPI_CPHA | SPI_CPOL | SPI_CS_HIGH | SPI_LSB_FIRST | SPI_3WIRE | SPI_LOOP | SPI_NO_CS | SPI_READY)

//============== LCD基本绘图方法 =============
#define AMOLED_WMODE _IOW('Q', 0x31, int)
#define AMOLED_XYSET _IOW('Q', 0x32, int)
#define AMOLED_DRAW _IOW('Q', 0x33, int)
#define AMOLED_BRIGHT _IOW('Q', 0x37, int)
//=============== dma设置方法 ===============
#define AMOLED_MODE_SET _IOW('Q', 0x34, int)
#define AMOLED_PERW_SET _IOW('Q', 0x35, int)
#define AMOLED_SPEED_SET _IOW('Q', 0x36, int)
//=============== 专用绘图方法 ================
#define AMOLED_FB_INIT _IOW('Q', 0x41, int)

//============= 屏幕基本参数 =============

//LCD屏尺寸 0~239
#define AMOLED_X_MAX 240
#define AMOLED_Y_MAX 240
#define AMOLED_DOT_MAX 172800 //240 * 240 * 3
#define AMOLED_DOT_PERB 8 //每字节bit数

//============= lcd基本结构体 =============

struct spidma_data
{
    u8 type; //屏幕类型: 0/amoled 1/lcd-st7789v2

    dev_t devt;
    spinlock_t spi_lock;
    struct spi_device *spi;
    struct list_head device_entry;

    // buffer is NULL unless this device is open (users > 0)
    struct mutex buf_lock;
    unsigned users;
    u8 *buffer;
};

//
typedef struct
{
    struct spidma_data *spidma;
    struct fb_info *info;
    struct clk *clk;
    int lcd_wiring_mode;
    //
    void __iomem *baseAddr;
    unsigned char clk_div;
} Spi_Struct;

//
typedef struct
{
    unsigned char workMode;            // 工作模式设置  // 0/初始化(亮屏)  1/亮屏  2/灭屏  3/wakeup  4/sleep  5/刷新屏幕  255/test
    unsigned short xyAddress[4];       // xStart  yStart xEnd  yEnd       // 在写像素数据时才写入
    unsigned int datLen;               // (xEnd - xStart + 1)*(yEnd - yStart + 1)*3
    unsigned char dat[AMOLED_DOT_MAX]; // 240*240*3 = 172800
} Led_Struct;

typedef struct
{
    Led_Struct led;
    Spi_Struct spi;
} Amoled_Struct;

#endif
