
#include <linux/types.h>
#include <linux/spi/amoled.h>
#include <linux/input/synaptics_dsx_v2_5_3.h>
#include <mach/gpio.h>
#include <mach/at91_pio.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#ifndef int8_t
#define int8_t char
#endif
#ifndef uint8_t
#define uint8_t unsigned char
#endif
#ifndef int16_t
#define int16_t short
#endif
#ifndef uint16_t
#define uint16_t unsigned short
#endif
#ifndef int32_t
#define int32_t int
#endif
#ifndef uint32_t
#define uint32_t unsigned int
#endif

#ifndef SLEEP_MILLI_SEC
#define SLEEP_MILLI_SEC(nMilliSec)               \
    do                                           \
    {                                            \
        int32_t timeout = (nMilliSec)*HZ / 1000; \
        while (timeout > 0)                      \
        {                                        \
            timeout = schedule_timeout(timeout); \
        }                                        \
    } while (0);
#endif

static Amoled_Struct *as = NULL;
static void __iomem *gpioc_base;

//使能触屏
#define TOUCH_ENABLE 1

//====================================== LCD部分 ======================================

//频率最高 132000000 Hz
#define AMOLED_MCK 44000000

//IO  //SPI1
#define AMOLED_IO_TE AT91_PIN_PA17
#define AMOLED_IO_DCX AT91_PIN_PA2
#define AMOLED_IO_SDI AT91_PIN_PC23
#define AMOLED_IO_SCL AT91_PIN_PC24
#define AMOLED_IO_CS AT91_PIN_PA4
#define AMOLED_IO_RES AT91_PIN_PA13
#define AMOLED_IO_1V8EN AT91_PIN_PD11
#define AMOLED_IO_3V3EN AT91_PIN_PA24

#define AMOLED_TE_IN() amoled_get_gpio_value(AMOLED_IO_TE)
// #define AMOLED_SCL_OUT(x) amoled_set_gpio_value(AMOLED_IO_SCL, x)
// #define AMOLED_SDI_IN()   amoled_get_gpio_value(AMOLED_IO_SDI)
// #define AMOLED_SDI_OUT(x) amoled_set_gpio_value(AMOLED_IO_SDI, x)
#define AMOLED_DCX_OUT(x) amoled_set_gpio_value(AMOLED_IO_DCX, x)
#define AMOLED_CSX_OUT(x) amoled_set_gpio_value(AMOLED_IO_CS, x)
#define AMOLED_RES_OUT(x) amoled_set_gpio_value(AMOLED_IO_RES, x)

#define AMOLED_1V8EN_OUT(x) //amoled_set_gpio_value(AMOLED_IO_1V8EN, x)
#define AMOLED_3V3EN_OUT(x) amoled_set_gpio_value(AMOLED_IO_3V3EN, x)

//SPI时序相关延时
#define AMOLED_T_CSV() schedule_timeout(1)  //10ns
#define AMOLED_T_SDI() schedule_timeout(1)  //10ns
#define AMOLED_T_SCL() schedule_timeout(1)  //10ns
#define AMOLED_T_SCH() schedule_timeout(1)  //10ns
#define AMOLED_T_BYTE() schedule_timeout(1) //10ns
//初始化相关延时
#define AMOLED_T_RES() SLEEP_MILLI_SEC(200)    //>10ms
#define AMOLED_T_SLPOUT() SLEEP_MILLI_SEC(200) //>120ms
#define AMOLED_T_CLK() SLEEP_MILLI_SEC(10) //>120ms

//LCD屏相关寄存器地址
#define COL_ADDR 0x2A
#define ROW_ADDR 0x2B
#define DAT_ADDR 0x2C

//====================================== GPIO ======================================

static int32_t amoled_request_gpio(int32_t gpio_index)
{
    char gpio_name[20];
    gpio_name[0] = '\0';
    snprintf(gpio_name, sizeof(gpio_name), "amoled_%d", gpio_index);
    if (gpio_request(gpio_index, gpio_name))
    {
        printk(KERN_ERR "amoled : request gpio %d failed.\n", gpio_index);
        return -EBUSY;
    }
    return 0;
}

static void amoled_free_gpio(int32_t gpio_index)
{
    gpio_free(gpio_index);
}

static void amoled_set_gpio_value(int32_t gpio_index, int32_t gpio_state)
{
    if (amoled_request_gpio(gpio_index))
    {
        printk(KERN_INFO "amoled : request_one_gpio false\n");
        return;
    }
    gpio_direction_output(gpio_index, gpio_state);
    amoled_free_gpio(gpio_index);
}

static uint8_t amoled_get_gpio_value(int32_t gpio_index)
{
    uint8_t ret = 0;
    if (amoled_request_gpio(gpio_index))
    {
        printk(KERN_INFO "amoled : request_one_gpio false\n");
        return;
    }
    // at91_set_GPIO_periph(gpio_index, 1);
    gpio_direction_input(gpio_index);
    ret = gpio_get_value(gpio_index);
    amoled_free_gpio(gpio_index);
    return ret;
}

//==================================== DMA调用 =====================================

#define AMOLED_DRAW_MODE 0 // 0: 手动或fb模式 1: 线程 50ms 自动刷屏

extern int32_t *touch_irq_value;

static void amoled_complete(void *arg)
{
    complete(arg);
}

static ssize_t amoled_sync(struct spidma_data *spidma, struct spi_message *message)
{
    DECLARE_COMPLETION_ONSTACK(done);
    int32_t status;

    message->complete = amoled_complete;
    message->context = &done;

#if (TOUCH_ENABLE)
    //关闭 touch IO 中断
    if (touch_irq_value)
        disable_irq(*touch_irq_value);
#endif

    spin_lock_irq(&spidma->spi_lock);
    if (spidma->spi == NULL)
        status = -ESHUTDOWN;
    else
        status = spi_async(spidma->spi, message);
    spin_unlock_irq(&spidma->spi_lock);

    if (status == 0)
    {
        wait_for_completion(&done);
        status = message->status;
        if (status == 0)
            status = message->actual_length;
    }

#if (TOUCH_ENABLE)
    //使能 touch IO 中断
    if (touch_irq_value)
        enable_irq(*touch_irq_value);
#endif

    return status;
}

static inline ssize_t amoled_sync_write(struct spidma_data *spidma, size_t len)
{
    struct spi_transfer t = {
        .tx_buf = spidma->buffer,
        .len = len,
        // .bits_per_word = AMOLED_DOT_PERB,
        // .speed_hz = AMOLED_MCK,
    };
    struct spi_message m;

    spi_message_init(&m);
    spi_message_add_tail(&t, &m);
    return amoled_sync(spidma, &m);
}

static inline ssize_t amoled_sync_read(struct spidma_data *spidma, size_t len)
{
    struct spi_transfer t = {
        .rx_buf = spidma->buffer,
        .len = len,
        // .bits_per_word = AMOLED_DOT_PERB,
        // .speed_hz = AMOLED_MCK,
    };
    struct spi_message m;

    spi_message_init(&m);
    spi_message_add_tail(&t, &m);
    return amoled_sync(spidma, &m);
}

//=================================== software spi ===================================

static void SDI_OUT(uint8_t v)
{
    __raw_writel(1 << 23, gpioc_base + (v ? PIO_SODR : PIO_CODR));
}
static uint8_t SDI_IN()
{
    return (__raw_readl(gpioc_base + PIO_PDSR) & (1 << 23)) ? 1 : 0;
}
static void SCL_OUT(uint8_t v)
{
    __raw_writel(1 << 24, gpioc_base + (v ? PIO_SODR : PIO_CODR));
}

static void amoled_spi_9bit_write(uint8_t dat, uint8_t dcx)
{
    uint8_t i;

    SCL_OUT(0);
    AMOLED_T_SCL();
    SDI_OUT(dcx);
    AMOLED_T_SDI();
    SCL_OUT(1);
    AMOLED_T_SCH();
    for (i = 0; i < 8; i++)
    {
        SCL_OUT(0);
        AMOLED_T_SCL();
        if ((dat << i) & 0x80)
            SDI_OUT(1);
        else
            SDI_OUT(0);
        AMOLED_T_SDI();
        SCL_OUT(1);
        AMOLED_T_SCH();
    }

    AMOLED_T_BYTE();
}

static uint8_t amoled_spi_9bit_read()
{
    uint8_t i, ret = 0;

    SCL_OUT(0);
    AMOLED_T_SCL();
    ret = SDI_IN();
    AMOLED_T_SDI();
    SCL_OUT(1);
    AMOLED_T_SCH();
    for (i = 0; i < 8; i++)
    {
        SCL_OUT(0);
        AMOLED_T_SCL();
        ret <<= 8;
        ret |= SDI_IN();
        AMOLED_T_SDI();
        SCL_OUT(1);
        AMOLED_T_SCH();
    }

    AMOLED_T_BYTE();

    return ret;
}

//==================================== LCD 公共 =====================================

// dcx : 信号位0/1    dat : 8位的数据或指令  mode : 0/cmd 1/dat
static void amoled_mode_write(struct spidma_data *spidma, uint8_t dat, uint8_t dcx)
{
    if (spidma->type == 0)
    {
        //片选使能
        AMOLED_CSX_OUT(0);
        AMOLED_T_CSV();
        //DCX
        AMOLED_DCX_OUT(dcx);
        spidma->buffer[0] = dat;
        amoled_sync_write(spidma, 1);
        //片选失能
        AMOLED_CSX_OUT(1);
        AMOLED_T_CSV();
    }
    else if (spidma->type == 1)
    {
        //注销spi-dma
        if (spidma->spi->bits_per_word != 0)
        {
            spidma->spi->bits_per_word = 0;
            //引脚复用为SPI
            __raw_writel(0x50494F00, gpioc_base + 0xE4); //关写保护
            __raw_writel(0x3 << 23, gpioc_base + 0x10);  //允许输出
            __raw_writel(0x3 << 23, gpioc_base + 0xA0);  //允许输出
            __raw_writel(0x3 << 23, gpioc_base + 0x00);  //启动IO功能
            // __raw_writel(0x3<<23, gpioc_base+0x64);//上拉
            // __raw_writel(0x50494F01, gpioc_base+0xE4);//开写保护
        }
        //片选使能
        AMOLED_CSX_OUT(0);
        AMOLED_T_CSV();
        //发出
        amoled_spi_9bit_write(dat, dcx);
        //片选失能
        AMOLED_CSX_OUT(1);
        AMOLED_T_CSV();
    }
}

static void amoled_write_byte(struct spidma_data *spidma, uint8_t addr, uint8_t dat)
{
    uint32_t i;
    if (spidma->type == 0)
    {
        //写地址
        amoled_mode_write(spidma, addr, 0);
        //写数据
        amoled_mode_write(spidma, dat, 1);
    }
    else if (spidma->type == 1)
    {
        //注销spi-dma
        if (spidma->spi->bits_per_word != 0)
        {
            spidma->spi->bits_per_word = 0;
            //引脚复用为SPI
            __raw_writel(0x50494F00, gpioc_base + 0xE4); //关写保护
            __raw_writel(0x3 << 23, gpioc_base + 0x10);  //允许输出
            __raw_writel(0x3 << 23, gpioc_base + 0xA0);  //允许输出
            __raw_writel(0x3 << 23, gpioc_base + 0x00);  //启动IO功能
            // __raw_writel(0x3<<23, gpioc_base+0x64);//上拉
            // __raw_writel(0x50494F01, gpioc_base+0xE4);//开写保护
        }
        //片选使能
        AMOLED_CSX_OUT(0);
        AMOLED_T_CSV();
        //写地址
        amoled_spi_9bit_write(addr, 0);
        //写数据
        amoled_spi_9bit_write(dat, 1);
        //片选失能
        AMOLED_CSX_OUT(1);
        AMOLED_T_CSV();
    }
}

static uint32_t amoled_read_bytes(struct spidma_data *spidma, uint8_t addr, uint8_t byteN)
{
    uint32_t i = byteN;
    uint32_t ret = 0;
    if (spidma->type == 1)
    {
        //注销spi-dma
        if (spidma->spi->bits_per_word != 0)
        {
            spidma->spi->bits_per_word = 0;
            //引脚复用为SPI
            __raw_writel(0x50494F00, gpioc_base + 0xE4); //关写保护
            __raw_writel(0x3 << 23, gpioc_base + 0x10);  //允许输出
            __raw_writel(0x3 << 23, gpioc_base + 0xA0);  //允许输出
            __raw_writel(0x3 << 23, gpioc_base + 0x00);  //启动IO功能
            // __raw_writel(0x3<<23, gpioc_base+0x64);//上拉
            // __raw_writel(0x50494F01, gpioc_base+0xE4);//开写保护
        }
    }
    //片选使能
    AMOLED_CSX_OUT(0);
    AMOLED_T_CSV();
    if (spidma->type == 0)
    {
        //写地址
        amoled_mode_write(spidma, addr, 0);
        //读数据
        amoled_sync_read(spidma, byteN);
    }
    else if (spidma->type == 1)
    {
        //写地址
        amoled_spi_9bit_write(addr, 0);
        //读数据
        for (i = 0; i < byteN; i++)
            spidma->buffer[i] = amoled_spi_9bit_read();
    }
    //片选失能
    AMOLED_CSX_OUT(1);
    AMOLED_T_CSV();
    //组装数据
    for (i = 0; i < byteN; i++)
    {
        ret <<= 8;
        ret |= spidma->buffer[i];
    }
    return ret;
}

static void amoled_write_rgb(struct spidma_data *spidma, uint8_t *dat, uint32_t datLen)
{
    uint32_t i = 0;
    if (spidma->type == 0)
    {
        memcpy(spidma->buffer, dat, datLen);
        //DCX
        AMOLED_DCX_OUT(1);
    }
    else if (spidma->type == 1)
    {
        //重新初始化
        if (spidma->spi->bits_per_word != 9)
        {
            //引脚复用为SPI
            __raw_writel(0x50494F00, gpioc_base + 0xE4); //关写保护
            __raw_writel(0x3 << 23, gpioc_base + 0x04);  //关闭IO功能
            // __raw_writel(0x50494F01, gpioc_base+0xE4);//开写保护
            //初始化
            spidma->spi->bits_per_word = 9;
            spi_setup(spidma->spi);
        }
        //RGB888转RGB565,记得再取反
        for (i = 0; i < datLen;)
        {
            spidma->buffer[i] = (~dat[i]) | 0x01;
            i += 1;
            spidma->buffer[i] = (~dat[i]) | 0x01;
            i += 1;
            spidma->buffer[i] = (~dat[i]) | 0x01;
            i += 1;
        }
        datLen = i;
    }
    //片选使能
    AMOLED_CSX_OUT(0);
    AMOLED_T_CSV();
    //写数据
    amoled_sync_write(spidma, datLen);
    //片选失能
    AMOLED_CSX_OUT(1);
    AMOLED_T_CSV();
}

static void amoled_write_rgb2(struct spidma_data *spidma, uint8_t rDat, uint8_t gDat, uint8_t bDat, uint32_t dotNum)
{
    uint32_t i = 0;
    if (spidma->type == 0)
    {
        for (i = 0; i < dotNum * 3;)
        {
            spidma->buffer[i++] = rDat;
            spidma->buffer[i++] = gDat;
            spidma->buffer[i++] = bDat;
        }
        //DCX
        AMOLED_DCX_OUT(1);
    }
    else if (spidma->type == 1)
    {
        //重新初始化
        if (spidma->spi->bits_per_word != 9)
        {
            //引脚复用为SPI
            __raw_writel(0x50494F00, gpioc_base + 0xE4); //关写保护
            __raw_writel(0x3 << 23, gpioc_base + 0x04);  //关闭IO功能
            // __raw_writel(0x50494F01, gpioc_base+0xE4);//开写保护
            //初始化
            spidma->spi->bits_per_word = 9;
            spi_setup(spidma->spi);
        }
        //转RGB565,记得再取反
        for (i = 0; i < dotNum * 3;)
        {
            spidma->buffer[i++] = (~rDat) | 0x01;
            spidma->buffer[i++] = (~gDat) | 0x01;
            spidma->buffer[i++] = (~bDat) | 0x01;
        }
    }
    //片选使能
    AMOLED_CSX_OUT(0);
    AMOLED_T_CSV();
    //写数据
    amoled_sync_write(spidma, i);
    //片选失能
    AMOLED_CSX_OUT(1);
    AMOLED_T_CSV();
}

static void amoled_address_set(struct spidma_data *spidma, uint8_t xStart, uint8_t yStart, uint8_t xEnd, uint8_t yEnd)
{
    //----- x地址 -----
    amoled_mode_write(spidma, COL_ADDR, 0);
    amoled_mode_write(spidma, xStart >> 8, 1);
    amoled_mode_write(spidma, xStart & 0xFF, 1);
    amoled_mode_write(spidma, xEnd >> 8, 1);
    amoled_mode_write(spidma, xEnd & 0xFF, 1);
    //----- y地址 -----
    amoled_mode_write(spidma, ROW_ADDR, 0);
    amoled_mode_write(spidma, yStart >> 8, 1);
    amoled_mode_write(spidma, yStart & 0xFF, 1);
    amoled_mode_write(spidma, yEnd >> 8, 1);
    amoled_mode_write(spidma, yEnd & 0xFF, 1);
}

static void amoled_cleer(struct spidma_data *spidma, uint16_t xStart, uint16_t yStart,
                         uint16_t xEnd, uint16_t yEnd, uint8_t rDat, uint8_t gDat, uint8_t bDat)
{
    uint32_t datLen;
    uint16_t xS = xStart, yS = yStart, xE = xEnd, yE = yEnd;
    uint16_t u16Temp;
    //范围检查
    if (xS >= AMOLED_X_MAX || yS >= AMOLED_Y_MAX || xE >= AMOLED_X_MAX || yE >= AMOLED_Y_MAX)
        return;
    //矩阵取点调整
    if (xS > xE && yS <= yE)
    {
        u16Temp = xS;
        xS = xE;
        xE = u16Temp; //x值互换
    }
    else if (yS > yE && xS <= xE)
    {
        u16Temp = yS;
        yS = yE;
        yE = u16Temp; //y值互换
    }
    else if (xS > xE && yS > yE)
    {
        u16Temp = xS;
        xS = xE;
        xE = u16Temp; //x值互换
        u16Temp = yS;
        yS = yE;
        yE = u16Temp; //y值互换
    }
    //使能高速SPI传输
    if (spidma->type == 0)
        amoled_mode_write(spidma, 0xC4, 0x81);
    //设置绘制范围
    amoled_address_set(spidma, xS, yS, xE, yE);
    //写地址
    amoled_mode_write(spidma, DAT_ADDR, 0);
    //写入全部像素点
    datLen = (xE - xS + 1) * (yE - yS + 1);
    //write data
    amoled_write_rgb2(spidma, rDat, gDat, bDat, datLen);
    //失能高速SPI传输
    if (spidma->type == 0)
        amoled_mode_write(spidma, 0xC4, 0x00);
}

static void amoled_draw(struct spidma_data *spidma, uint16_t xStart, uint16_t yStart,
                        uint16_t xEnd, uint16_t yEnd, uint8_t *pic)
{
    int32_t i, j;
    uint32_t datLen;
    uint16_t xS = xStart, yS = yStart, xE = xEnd, yE = yEnd;
    uint16_t u16Temp;
    //范围检查
    if (xS >= AMOLED_X_MAX || yS >= AMOLED_Y_MAX || xE >= AMOLED_X_MAX || yE >= AMOLED_Y_MAX)
        return;
    //矩阵取点调整
    if (xS > xE && yS <= yE)
    {
        u16Temp = xS;
        xS = xE;
        xE = u16Temp; //x值互换
    }
    else if (yS > yE && xS <= xE)
    {
        u16Temp = yS;
        yS = yE;
        yE = u16Temp; //y值互换
    }
    else if (xS > xE && yS > yE)
    {
        u16Temp = xS;
        xS = xE;
        xE = u16Temp; //x值互换
        u16Temp = yS;
        yS = yE;
        yE = u16Temp; //y值互换
    }
    //使能高速SPI传输
    if (spidma->type == 0)
        amoled_mode_write(spidma, 0xC4, 0x81);
    //设置绘制范围
    amoled_address_set(spidma, xS, yS, xE, yE);
    //写地址
    amoled_mode_write(spidma, DAT_ADDR, 0);
    //写入全部像素点
    datLen = (xE - xS + 1) * (yE - yS + 1) * 3;
    //write data
    amoled_write_rgb(spidma, pic, datLen);
    //失能高速SPI传输
    if (spidma->type == 0)
        amoled_mode_write(spidma, 0xC4, 0x00);
}

static int32_t amoled_setup(struct spidma_data *spidma)
{
    //printk(KERN_INFO "--FUN-- amoled_init\r\n");

    //识别L屏幕类型
    if (AMOLED_TE_IN() == 1)
        spidma->type = 1;
    else
        spidma->type = 0;

    if (spidma->type == 0)
    {
        spidma->spi->bits_per_word = AMOLED_DOT_PERB;
        //spi初始化
        spi_setup(spidma->spi);
        //复位开始
        //printk(KERN_INFO "--FUN-- amoled_init : reset start\r\n");
        AMOLED_RES_OUT(0);
        AMOLED_T_RES();
        //上电使能
        AMOLED_1V8EN_OUT(1);
        //背光使能
        AMOLED_3V3EN_OUT(1);
        AMOLED_T_RES();
        //复位结束
        //printk(KERN_INFO "--FUN-- amoled_init : reset end\r\n");
        AMOLED_RES_OUT(1);
        AMOLED_T_RES();
        //初始化配置
        //printk(KERN_INFO "--FUN-- amoled_init : init setting\r\n");
        amoled_write_byte(spidma, 0xFE, 0x00);
        amoled_write_byte(spidma, 0x35, 0x00);
        amoled_write_byte(spidma, 0x53, 0x20);
        //sleep out
        //printk(KERN_INFO "--FUN-- amoled_init : sleep out\r\n");
        amoled_write_byte(spidma, 0x11, 0x00);
        AMOLED_T_SLPOUT();
        //dispon
        //printk(KERN_INFO "--FUN-- amoled_init : display on\r\n");
        amoled_write_byte(spidma, 0x29, 0x00);
        //High Brightness ON
        //amoled_write_byte(spidma, 0x66, 0x02);
        //Idle Mode ON
        //amoled_write_byte(spidma, 0x39, 0x00);
        //Brightness Control
        amoled_write_byte(spidma, 0x51, 0xFF); //0x00 ~0xFF
    }
    else if (spidma->type == 1)
    {
        spidma->spi->bits_per_word = 9;
        //spi初始化
        spi_setup(spidma->spi);
        // 上电
        AMOLED_1V8EN_OUT(1);
        AMOLED_3V3EN_OUT(1);
        // 关闭背光
        AMOLED_DCX_OUT(0);
        // 复位
        AMOLED_RES_OUT(0);
        AMOLED_T_RES();
        AMOLED_RES_OUT(1);
        AMOLED_T_RES();
        // 复位后必须读一次3字节的id,否则不能运行
        amoled_read_bytes(spidma, 0x04, 3);
        /* Sleep Out */
        amoled_mode_write(spidma, 0x11, 0);
        /* wait for power stability */
        AMOLED_T_SLPOUT();
        /* 65K RGB 5-6-5-bit  */
        amoled_write_byte(spidma, 0x3A, 0x66);
        /* 60MHz  */
        amoled_cleer(spidma, 0, 0, AMOLED_X_MAX - 1, AMOLED_Y_MAX - 1, 0, 0, 0);
        /* display on */
        amoled_mode_write(spidma, 0x29, 0);
        // 开启背光
        AMOLED_DCX_OUT(1);
    }

    //printk(KERN_INFO "--FUN-- amoled_init : finial\r\n");
    return 0;
}

static int32_t amoled_work_mode(struct spidma_data *spidma, uint8_t mode, uint8_t *pic)
{
    printk(KERN_INFO "--FUN-- amoled_work_mode : mode/%d\r\n", mode);
    // 0/初始化(亮屏)  1/亮屏  2/灭屏  3/sleep  4/wakeup  5/powerOff
    switch (mode)
    {
        //===== 0/初始化(亮屏) =====
    case 0:
        //寄存器初始化
        if (amoled_setup(spidma) < 0)
            return -1;
        break;
        //===== 1/亮屏 =====
    case 1:
        amoled_write_byte(spidma, 0x29, 0x00);
        // 开启背光
        if(spidma->type == 1)
            AMOLED_DCX_OUT(1);
        break;
        //===== 2/灭屏 =====
    case 2:
        amoled_write_byte(spidma, 0x28, 0x00);
        // 关闭背光
        if(spidma->type == 1)
            AMOLED_DCX_OUT(0);
        break;
        //===== 3/wakeup =====
    case 3:
        amoled_write_byte(spidma, 0x11, 0x00); //sleep out
        AMOLED_T_SLPOUT();
        amoled_write_byte(spidma, 0x29, 0x00); //dispon
        break;
        //===== 4/sleep =====
    case 4:
        amoled_write_byte(spidma, 0x28, 0x00); //dispoff
        amoled_write_byte(spidma, 0x10, 0x00); //sleep
        AMOLED_T_SLPOUT();
        break;
        //===== 5/powerOff =====
    case 5:
        amoled_write_byte(spidma, 0x28, 0x00); //dispoff
        amoled_write_byte(spidma, 0x10, 0x00); //sleep
        AMOLED_T_SLPOUT();
        AMOLED_3V3EN_OUT(0); //背光失能
        AMOLED_1V8EN_OUT(0); //上电失能
        break;
        //===== 6/刷新屏幕 =====
    case 6:
        if (pic == NULL)
            return -1;
        amoled_draw(spidma, 0, 0, AMOLED_X_MAX - 1, AMOLED_Y_MAX - 1, pic);
        break;
        //===== 255/test =====
    case 255:
        amoled_write_byte(spidma, 0xAB, 0x55);
        break;
        //========================
    default:
        break;
    }
    return 0;
}

//-------------------- 驱动部分
/*
 * This supports access to SPI devices using normal userspace I/O calls.
 * Note that while traditional UNIX/POSIX I/O semantics are half duplex,
 * and often mask message boundaries, full SPI support requires full duplex
 * transfers.  There are several kinds of internal message boundaries to
 * handle chipselect management and other protocol options.
 *
 * SPI has a character major number assigned.  We allocate minor numbers
 * dynamically using a bitmask.  You must use hotplug tools, such as udev
 * (or mdev with busybox) to create and destroy the /dev/amoledB.C device
 * nodes, since there is no fixed association of minor numbers with any
 * particular SPI bus or device.
 */
#define AMOLED_MAJOR 154 /* assigned */
#define N_SPI_MINORS 1   /* ... up to 256 */
#define AMOLED_DEV_NAME "amoled_spi"

static DECLARE_BITMAP(minors, N_SPI_MINORS);

static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

static uint32_t bufsiz = AMOLED_DOT_MAX;
module_param(bufsiz, uint, S_IRUGO);
MODULE_PARM_DESC(bufsiz, "data bytes in biggest supported SPI message");

static ssize_t amoled_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
static ssize_t amoled_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
static int32_t amoled_ioctl(struct file *filp, uint32_t cmd, uint32_t arg);
static int32_t amoled_open(struct inode *inode, struct file *filp);
static int32_t amoled_release(struct inode *inode, struct file *filp);

static const struct file_operations amoled_fops = {
    .owner = THIS_MODULE,
    /* REVISIT switch to aio primitives, so that userspace
     * gets more complete API coverage.  It'll simplify things
     * too, except for the locking.
     */
    .write = amoled_write,
    .read = amoled_read,
    .unlocked_ioctl = amoled_ioctl,
    .open = amoled_open,
    .release = amoled_release,
    .llseek = no_llseek,
};

//-------------------- fb0

/*-------------------------------------------------------------------------*/

/* The main reason to have this class is to make mdev/udev create the
 * /dev/amoledB.C character device nodes exposing our userspace API.
 * It also simplifies memory management.
 */
#define ATMEL_LCDFB_FBINFO_DEFAULT (FBINFO_DEFAULT | FBINFO_PARTIAL_PAN_OK | FBINFO_HWACCEL_YPAN)
#define AMOLED_HARDWARE_MODE_3_1 0 //三线模式一
#define AMOLED_HARDWARE_MODE_3_2 1 //三线模式二
#define AMOLED_HARDWARE_MODE_3_3 2 //三线模式三
#define AMOLED_HARDWARE_MODE_4_1 3 //四线模式一
#define AMOLED_HARDWARE_MODE_4_2 4 //四线模式二
#define AMOLED_HARDWARE_MODE_4_3 5 //四线模式三

#define AMOLED_LCDC_WIRING_RGB 0
#define AMOLED_LCDC_WIRING_RGB555 1

struct amoled_lcdfb_info
{
    struct clk *spi_clk;
    int32_t lcd_wiring_mode;
    struct fb_info *info;
    struct spidma_data *spidma;
};
static const struct fb_videomode *amoled_lcdfb_choose_mode(struct fb_var_screeninfo *var,
                                                           struct fb_info *info)
{
    struct fb_videomode varfbmode;
    const struct fb_videomode *fbmode = NULL;

    printk(KERN_INFO "--FUN-- amoled_lcdfb_choose_mode\r\n");

    fb_var_to_videomode(&varfbmode, var);
    fbmode = fb_find_nearest_mode(&varfbmode, &info->modelist);
    if (fbmode)
        fb_videomode_to_var(var, fbmode);
    return fbmode;
}
static int32_t amoled_lcdfb_check_var(struct fb_var_screeninfo *var,
                                      struct fb_info *info)
{
    struct device *dev = info->device;
    Amoled_Struct *as = info->par;
    uint32_t clk_value_khz;

    printk(KERN_INFO "--FUN-- amoled_lcdfb_check_var\r\n");

    clk_value_khz = clk_get_rate(as->spi.clk) / 1000;

    dev_info(dev, "%s:\n", __func__);

    if (!(var->pixclock && var->bits_per_pixel))
    {
        /* choose a suitable mode if possible */
        if (!amoled_lcdfb_choose_mode(var, info))
        {
            dev_err(dev, "needed value not specified\n");
            return -EINVAL;
        }
    }

    dev_info(dev, "  resolution: %ux%u\n", var->xres, var->yres);
    dev_info(dev, "  pixclk:     %lu KHz\n", PICOS2KHZ(var->pixclock));
    dev_info(dev, "  bpp:        %u\n", var->bits_per_pixel);
    dev_info(dev, "  clk:        %lu KHz\n", clk_value_khz);

    if (PICOS2KHZ(var->pixclock) > clk_value_khz)
    {
        dev_err(dev, "%lu KHz pixel clock is too fast\n", PICOS2KHZ(var->pixclock));
        return -EINVAL;
    }

    /* Do not allow to have real resoulution larger than virtual */
    if (var->xres > var->xres_virtual)
        var->xres_virtual = var->xres;

    if (var->yres > var->yres_virtual)
        var->yres_virtual = var->yres;

    /* Force same alignment for each line */
    var->xres = (var->xres + 3) & ~3UL;
    var->xres_virtual = (var->xres_virtual + 3) & ~3UL;

    var->red.msb_right = var->green.msb_right = var->blue.msb_right = 0;
    var->transp.msb_right = 0;
    var->transp.offset = var->transp.length = 0;
    var->xoffset = var->yoffset = 0;

    if (info->fix.smem_len)
    {
        uint32_t smem_len = (var->xres_virtual * var->yres_virtual * ((var->bits_per_pixel + 7) / 8));
        if (smem_len > info->fix.smem_len)
        {
            dev_err(dev, "not enough memory for this mode\n");
            return -EINVAL;
        }
    }

    /* Saturate vertical and horizontal timings at maximum values */
    //  if (sinfo->dev_data->limit_screeninfo)
    //      sinfo->dev_data->limit_screeninfo(var);

    /* Some parameters can't be zero */
    var->vsync_len = max_t(u32, var->vsync_len, 1);
    var->right_margin = max_t(u32, var->right_margin, 1);
    var->hsync_len = max_t(u32, var->hsync_len, 1);
    var->left_margin = max_t(u32, var->left_margin, 1);

    switch (var->bits_per_pixel)
    {
    case 1:
    case 2:
    case 4:
    case 8:
        var->red.offset = var->green.offset = var->blue.offset = 0;
        var->red.length = var->green.length = var->blue.length = var->bits_per_pixel;
        break;
    case 15:
    case 16:
        if (as->spi.lcd_wiring_mode == AMOLED_LCDC_WIRING_RGB)
        {
            /* RGB:565 mode */
            var->red.offset = 11;
            var->blue.offset = 0;
            var->green.length = 6;
        }
        else if (as->spi.lcd_wiring_mode == AMOLED_LCDC_WIRING_RGB555)
        {
            var->red.offset = 10;
            var->blue.offset = 0;
            var->green.length = 5;
        }
        else
        {
            /* BGR:555 mode */
            var->red.offset = 0;
            var->blue.offset = 10;
            var->green.length = 5;
        }
        var->green.offset = 5;
        var->red.length = var->blue.length = 5;
        break;
    case 32:
        var->transp.offset = 24;
        var->transp.length = 8;
        /* fall through */
    case 24:
        if (as->spi.lcd_wiring_mode == AMOLED_LCDC_WIRING_RGB)
        {
            /* RGB:888 mode */
            var->red.offset = 16;
            var->blue.offset = 0;
        }
        else
        {
            /* BGR:888 mode */
            var->red.offset = 0;
            var->blue.offset = 16;
        }
        var->green.offset = 8;
        var->red.length = var->green.length = var->blue.length = 8;
        break;
    default:
        dev_err(dev, "color depth %d not supported\n",
                var->bits_per_pixel);
        return -EINVAL;
    }

    return 0;
}
static int32_t amoled_lcdfb_set_par(struct fb_info *info)
{

    printk(KERN_INFO "--FUN-- amoled_lcdfb_set_par\r\n");

    //  printk("amoled_lcdfb_set_par\n");
    return 0;
}
static int32_t amoled_lcdfb_setcolreg(uint32_t regno, uint32_t red, uint32_t green,
                                      uint32_t blue, uint32_t transp, struct fb_info *info)
{

    //printk(KERN_INFO "--FUN-- amoled_lcdfb_setcolreg\r\n");

    return 0;
}
static int32_t amoled_lcdfb_blank(int32_t blank, struct fb_info *info)
{

    printk(KERN_INFO "--FUN-- amoled_lcdfb_blank\r\n");

    return 0;
}
static int32_t amoled_lcdfb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{

    printk(KERN_INFO "--FUN-- amoled_lcdfb_pan_display\r\n");

    return 0;
}
void amoled_cfb_fillrect(struct fb_info *info, const struct fb_fillrect *rect)
{

    printk(KERN_INFO "--FUN-- amoled_cfb_fillrect\r\n");
}
void amoled_cfb_copyarea(struct fb_info *info, const struct fb_copyarea *region)
{

    printk(KERN_INFO "--FUN-- amoled_cfb_copyarea\r\n");
}
void amoled_cfb_imageblit(struct fb_info *info, const struct fb_image *image)
{
    u8 __iomem *ptr = info->screen_base;
    //Amoled_Struct *as = info->par;

    //printk(KERN_INFO "--FUN-- amoled_cfb_imageblit : data / %.2X, %.2X, %.2X, %.2X\r\n", ptr[0], ptr[1], ptr[2], ptr[3]);

    amoled_draw(as->spi.spidma, 0, 0, AMOLED_X_MAX - 1, AMOLED_Y_MAX - 1, ptr);

    if (*ptr == '1')
    {
        printk("screen_base 0 is:%s\n", ptr);
        *ptr = 0;
    }

    //  printk("amoled_cfb_imageblit\n");
}
static struct fb_ops amoled_lcdfb_ops = {
    .owner = THIS_MODULE,
    .fb_check_var = amoled_lcdfb_check_var,
    .fb_set_par = amoled_lcdfb_set_par,
    .fb_setcolreg = amoled_lcdfb_setcolreg,
    .fb_blank = amoled_lcdfb_blank,
    .fb_pan_display = amoled_lcdfb_pan_display,
    .fb_fillrect = amoled_cfb_fillrect,
    .fb_copyarea = amoled_cfb_copyarea,
    .fb_imageblit = amoled_cfb_imageblit,
};
static int32_t amoled_alloc_video_memory(struct fb_info *info)
{
    //struct fb_var_screeninfo *var = &info->var;
    //uint32_t smem_len;

    printk(KERN_INFO "--FUN-- amoled_alloc_video_memory\r\n");

    info->screen_base = dma_alloc_writecombine(info->dev, info->fix.smem_len,
                                               (dma_addr_t *)&info->fix.smem_start, GFP_KERNEL);

    if (!info->screen_base)
    {
        return -ENOMEM;
    }

    memset(info->screen_base, 0, info->fix.smem_len);

    return 0;
}
static int32_t __init amoled_init_fbinfo(struct fb_info *info)
{
    int32_t ret = 0;

    printk(KERN_INFO "--FUN-- amoled_init_fbinfo\r\n");

    info->var.activate |= FB_ACTIVATE_FORCE | FB_ACTIVATE_NOW;

    dev_info(info->device,
             "%luKiB frame buffer at %08lx (mapped at %p)\n",
             (ulong)info->fix.smem_len / 1024,
             (ulong)info->fix.smem_start,
             info->screen_base);

    /* Allocate colormap */
    ret = fb_alloc_cmap(&info->cmap, 256, 0);
    if (ret < 0)
        dev_err(info->device, "Alloc color map failed\n");

    return ret;
}
int32_t amoled_set_fbvar(struct fb_info *info)
{

    printk(KERN_INFO "--FUN-- amoled_set_fbvar\r\n");

    info->var.xres = AMOLED_X_MAX;
    info->var.yres = AMOLED_Y_MAX;
    //  info->var.xres_virtual = AMOLED_X_MAX;
    //  info->var.yres_virtual = AMOLED_Y_MAX;
    info->var.xoffset = 0;
    info->var.yoffset = 0;
    info->var.bits_per_pixel = AMOLED_DOT_PERB;

    //  info->var.red.offset = 16;
    //  info->var.red.length = 8;
    //  info->var.red.msb_right = 0;
    //  info->var.green.offset = 8;
    //  info->var.green.length = 8;
    //  info->var.green.msb_right = 0;
    //  info->var.blue.offset = 0;
    //  info->var.blue.length = 8;
    //  info->var.blue.msb_right = 0;

    info->var.pixclock = 578704;
    info->var.sync = 0;
    info->var.vmode = FB_VMODE_NONINTERLACED;
    info->var.activate = FB_ACTIVATE_NOW;
    return 0;
}

static int32_t amoled_framebuffer(Amoled_Struct *as)
{
    int32_t ret = 0;
    struct fb_info *info;
    //struct fb_videomode fbmode;

    printk(KERN_INFO "--FUN-- amoled_framebuffer\r\n");

    info = framebuffer_alloc(0, &as->spi.spidma->spi->dev);
    if (!info)
    {
        dev_err(&as->spi.spidma->spi->dev, "cannot allocate memory\n");
    }

    as->spi.clk = clk_get(&as->spi.spidma->spi->dev, "spi1_clk");
    if (IS_ERR(as->spi.clk))
    {
        ret = PTR_ERR(as->spi.clk);
    };
    as->spi.info = info;

    info->par = as;
    amoled_set_fbvar(info);
    strcpy(info->fix.id, "zhd_amoled");
    info->fix.smem_len = AMOLED_DOT_MAX;
    info->flags = ATMEL_LCDFB_FBINFO_DEFAULT;
    info->fbops = &amoled_lcdfb_ops;

    ret = amoled_alloc_video_memory(info);
    if (ret < 0)
    {
        dev_err(&as->spi.spidma->spi->dev, "cannot allocate framebuffer: %d\n", ret);
    }
    amoled_init_fbinfo(info);

    amoled_lcdfb_check_var(&info->var, info);

    ret = fb_set_var(info, &info->var);
    if (ret)
    {
        dev_warn(&as->spi.spidma->spi->dev, "unable to set display parameters\n");
    }

    //  fb_var_to_videomode(&fbmode, &info->var);
    //  fb_add_videomode(&fbmode, &info->modelist);

    register_framebuffer(info);
    printk("here end\n");
    return 0;
}

//==================================== 驱动接口 =====================================

/* Read-only message with current device setup */
static ssize_t amoled_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    Amoled_Struct *as;
    struct spidma_data *spidma;
    ssize_t status = 0;

    /* chipselect only toggles at start or end of operation */
    if (count > bufsiz)
        return -EMSGSIZE;

    as = filp->private_data;
    spidma = as->spi.spidma;

    mutex_lock(&spidma->buf_lock);
    status = amoled_sync_read(spidma, count);
    if (status > 0)
    {
        uint32_t missing;

        missing = copy_to_user(buf, spidma->buffer, status);
        if (missing == status)
            status = -EFAULT;
        else
            status = status - missing;
    }
    mutex_unlock(&spidma->buf_lock);

    return status;
}

/* Write-only message with current device setup */
static ssize_t amoled_write(struct file *filp, const char __user *buf,
                            size_t count, loff_t *f_pos)
{
    Amoled_Struct *as;
    struct spidma_data *spidma;
    ssize_t status = 0;
    uint32_t missing;

    /* chipselect only toggles at start or end of operation */
    if (count > bufsiz)
        return -EMSGSIZE;

    as = filp->private_data;
    spidma = as->spi.spidma;

    mutex_lock(&spidma->buf_lock);
    missing = copy_from_user(spidma->buffer, buf, count);
    if (missing == 0)
        status = amoled_sync_write(spidma, count);
    else
        status = -EFAULT;
    mutex_unlock(&spidma->buf_lock);

    return status;
}

static int32_t amoled_ioctl(struct file *filp, uint32_t cmd, uint32_t arg)
{
    int32_t retval = 0;
    struct spidma_data *spidma;
    struct spi_device *spi;
    uint32_t tmp;
    int32_t datLen = AMOLED_X_MAX * AMOLED_Y_MAX * 3;
    //
    Amoled_Struct *as;
    uint16_t *xyAddr = NULL;
    //
    as = filp->private_data;
    spidma = as->spi.spidma;
    spin_lock_irq(&spidma->spi_lock);
    spi = spi_dev_get(spidma->spi);
    spin_unlock_irq(&spidma->spi_lock);

    if (spi == NULL)
        return -ESHUTDOWN;

    /* use the buffer lock here for triple duty:
     *  - prevent I/O (from us) so calling spi_setup() is safe;
     *  - prevent concurrent SPI_IOC_WR_* from morphing
     *    data fields while SPI_IOC_RD_* reads them;
     *  - SPI_IOC_MESSAGE needs the buffer locked "normally".
     */
    mutex_lock(&spidma->buf_lock);

    switch (cmd)
    {
    //============================ Frame Buffer 模式启动 ============================
    case AMOLED_FB_INIT:
        if (as->spi.info == NULL)
            retval = amoled_framebuffer(as);
        else
            retval = 0;
        break;

    //================================ 工作模式设置 ================================
    case AMOLED_WMODE:
        printk(KERN_INFO "--FUN-- ioctl AMOLED_WMODE : %d\r\n", (uint8_t)(arg & 0xff));
        retval = amoled_work_mode(spidma, (uint8_t)(arg & 0xff), &as->led.dat[4]);
        break;
    //================================ 绘制坐标设置 ================================
    case AMOLED_XYSET:
        //拷贝数据
        xyAddr = (uint16_t *)kmalloc(sizeof(as->led.xyAddress), GFP_KERNEL);
        if (copy_from_user(xyAddr, (uint16_t *)arg, sizeof(as->led.xyAddress)))
        {
            printk(KERN_ERR "atmel_amoled AMOLED_XYSET : copy datas from user failed\r\n");
            kfree(xyAddr);
            retval = -datLen; //失败返回当前的像素点字节数
            break;
        }
        printk(KERN_INFO "--FUN-- ioctl AMOLED_XYSET : %d, %d, %d, %d\r\n", xyAddr[0], xyAddr[1], xyAddr[2], xyAddr[3]);
        //检查坐标有效性
        if (xyAddr[0] >= AMOLED_X_MAX || xyAddr[1] >= AMOLED_Y_MAX ||
            xyAddr[2] >= AMOLED_X_MAX || xyAddr[3] >= AMOLED_Y_MAX)
        {
            printk(KERN_ERR "atmel_amoled AMOLED_XYSET : xy addr err\r\n");
            kfree(xyAddr);
            retval = -datLen; //失败返回当前的像素点字节数
            break;
        }
        //拷贝到结构体
        if (xyAddr[0] > xyAddr[2] && xyAddr[1] <= xyAddr[3]) //x值互换
        {
            as->led.xyAddress[0] = xyAddr[2];
            as->led.xyAddress[1] = xyAddr[1];
            as->led.xyAddress[2] = xyAddr[0];
            as->led.xyAddress[3] = xyAddr[3];
        }
        else if (xyAddr[1] > xyAddr[3] && xyAddr[0] <= xyAddr[2]) //y值互换
        {
            as->led.xyAddress[0] = xyAddr[0];
            as->led.xyAddress[1] = xyAddr[3];
            as->led.xyAddress[2] = xyAddr[2];
            as->led.xyAddress[3] = xyAddr[1];
        }
        else if (xyAddr[0] > xyAddr[2] && xyAddr[1] > xyAddr[3]) //x值互换 y值互换
        {
            as->led.xyAddress[0] = xyAddr[2];
            as->led.xyAddress[1] = xyAddr[3];
            as->led.xyAddress[2] = xyAddr[0];
            as->led.xyAddress[3] = xyAddr[1];
        }
        else
            memcpy(as->led.xyAddress, xyAddr, sizeof(as->led.xyAddress));
        kfree(xyAddr);
        //计算像素点数据字节数(注意乘3)
        retval = (as->led.xyAddress[2] - as->led.xyAddress[0] + 1) * (as->led.xyAddress[3] - as->led.xyAddress[1] + 1) * 3;
        break;

    //================================ 像素数据传输 ================================
    case AMOLED_DRAW:
        //printk(KERN_INFO "--FUN-- ioctl AMOLED_DRAW\r\n");
        if (copy_from_user(as->led.dat, (uint8_t *)arg, datLen))
        {
            printk(KERN_ERR "atmel_amoled AMOLED_DRAW : copy datas from user failed\r\n");
            retval = -datLen; //失败返回当前的像素点字节数
            break;
        }
        //
#if (AMOLED_DRAW_MODE == 1)
        ;
#else
        amoled_draw(spidma, as->led.xyAddress[0], as->led.xyAddress[1], as->led.xyAddress[2], as->led.xyAddress[3], as->led.dat);
#endif
        retval = datLen;
        break;

    //================================== 亮度设置 ==================================
    case AMOLED_BRIGHT:
        printk(KERN_INFO "--FUN-- ioctl AMOLED_BRIGHT : %d\r\n", (uint8_t)(arg & 0xff));
        amoled_write_byte(spidma, 0x51, (uint8_t)(arg & 0xff)); //0x00 ~0xFF
        retval = 0;
        break;

    //================================= SPI模式设置 =================================
    case AMOLED_MODE_SET:
        retval = __get_user(tmp, (u8 __user *)arg);
        if (retval == 0)
        {
            u8 save = spi->mode;

            if (tmp & ~SPI_MODE_MASK)
            {
                retval = -EINVAL;
                break;
            }

            tmp |= spi->mode & ~SPI_MODE_MASK;
            spi->mode = (u8)tmp;
            retval = spi_setup(spi);
            if (retval < 0)
                spi->mode = save;
            else
                dev_dbg(&spi->dev, "spi mode %02x\n", tmp);
        }
        break;

    //============================== SPI一字节位数设置 ==============================
    case AMOLED_PERW_SET:
        retval = __get_user(tmp, (__u8 __user *)arg);
        if (retval == 0)
        {
            u8 save = spi->bits_per_word;

            spi->bits_per_word = tmp;
            retval = spi_setup(spi);
            if (retval < 0)
                spi->bits_per_word = save;
            else
                dev_dbg(&spi->dev, "%d bits per word\n", tmp);
        }
        break;

    //================================= SPI频率设置 =================================
    case AMOLED_SPEED_SET:
        retval = __get_user(tmp, (__u32 __user *)arg);
        if (retval == 0)
        {
            u32 save = spi->max_speed_hz;

            spi->max_speed_hz = tmp;
            retval = spi_setup(spi);
            if (retval < 0)
                spi->max_speed_hz = save;
            else
                dev_dbg(&spi->dev, "%d Hz (max)\n", tmp);
        }
        break;

    default:
        break;
    }

    mutex_unlock(&spidma->buf_lock);
    spi_dev_put(spi);
    return retval;
}
//
#if (AMOLED_DRAW_MODE == 1)

#ifndef SLEEP_MILLI_MS
#define SLEEP_MILLI_MS(nMilliSec)                \
    do                                           \
    {                                            \
        int32_t timeout = (nMilliSec)*HZ / 1000; \
        while (timeout > 0)                      \
        {                                        \
            timeout = schedule_timeout(timeout); \
        }                                        \
    } while (0);
#endif

static DECLARE_WAIT_QUEUE_HEAD(amoled_waitqueue);
static int32_t amoled_task;
static int32_t amoledAllow = 1;
static int32_t amoled_threadFun(void *argv)
{
    struct spidma_data *spidma = (struct spidma_data *)argv;

    DECLARE_WAITQUEUE(wait, current);
    daemonize("amoled");
    allow_signal(SIGKILL);
    add_wait_queue(&amoled_waitqueue, &wait);
    printk("Creat amoled thread!\r\n");
    while (amoledAllow)
    {
        set_current_state(TASK_INTERRUPTIBLE);
        //
        amoled_draw(spidma, 0, 0, AMOLED_X_MAX - 1, AMOLED_Y_MAX - 1, as->led.dat);
        //
        SLEEP_MILLI_MS(50); // 20Hz
        //
        if (signal_pending(current))
        {
            break;
        }
    }
    set_current_state(TASK_RUNNING);
    remove_wait_queue(&amoled_waitqueue, &wait);
    return 0;
}

#endif
//
static int32_t amoled_open(struct inode *inode, struct file *filp)
{
    int32_t status = 0; // -ENXIO;

    // 只初始化1次总结构体, 以后不再关闭和释放
    if (as != NULL)
    {
        filp->private_data = as;
        nonseekable_open(inode, filp);
        // 默认spi参数
        as->spi.spidma->spi->bits_per_word = AMOLED_DOT_PERB;
        as->spi.spidma->spi->max_speed_hz = AMOLED_MCK; //44000000;
        as->spi.spidma->spi->mode = SPI_MODE_MASK;
        // 默认全屏
        as->led.workMode = 0;
        as->led.xyAddress[0] = 0;
        as->led.xyAddress[1] = 0;
        as->led.xyAddress[2] = AMOLED_X_MAX - 1;
        as->led.xyAddress[3] = AMOLED_Y_MAX - 1;
        as->led.datLen = AMOLED_DOT_MAX;
        // 初始化LCD
        amoled_work_mode(as->spi.spidma, 0, NULL);
        return 0;
    }
    /*
    // 总的参数结构体初始化
    as = (Amoled_Struct *)kmalloc(sizeof(Amoled_Struct), GFP_KERNEL);
    memset(as, 0, sizeof(Amoled_Struct));
    // 互斥锁
    mutex_lock(&device_list_lock);

    list_for_each_entry(as->spi.spidma, &device_list, device_entry) {
        if (as->spi.spidma->devt == inode->i_rdev) {
            status = 0;
            break;
        }
    }
    if (status == 0) {
        if (!as->spi.spidma->buffer) {
            as->spi.spidma->buffer = kmalloc(bufsiz, GFP_KERNEL);
            if (!as->spi.spidma->buffer) {
                dev_dbg(&as->spi.spidma->spi->dev, "open/ENOMEM\n");
                status = -ENOMEM;
            }
        }
        if (status == 0) {
            as->spi.spidma->users++;
            filp->private_data = as;    //总的参数结构体传入到private_data, 方便其它接口函数调用
            nonseekable_open(inode, filp);
        }
    } else
        pr_debug("amoled: nothing for minor %d\n", iminor(inode));
    // 互斥解锁
    mutex_unlock(&device_list_lock);

    // 默认spi参数
    as->spi.spidma->spi->bits_per_word = AMOLED_DOT_PERB;
    as->spi.spidma->spi->max_speed_hz = AMOLED_MCK;   //44000000;
    as->spi.spidma->spi->mode = 0;
    // 默认全屏
    as->led.workMode = 0;
    as->led.xyAddress[0] = 0;
    as->led.xyAddress[1] = 0;
    as->led.xyAddress[2] = AMOLED_X_MAX - 1;
    as->led.xyAddress[3] = AMOLED_Y_MAX - 1;
    as->led.datLen = AMOLED_DOT_MAX;
    // 初始化LCD
    amoled_work_mode(as->spi.spidma, 0, NULL);
    // logo
    amoled_draw(as->spi.spidma, as->led.xyAddress[0], as->led.xyAddress[1], as->led.xyAddress[2], as->led.xyAddress[3], logo_buffer2);
    
    //amoled_framebuffer(as);

#if(AMOLED_DRAW_MODE == 1)
    amoled_task = kernel_thread(amoled_threadFun, (void *)(as->spi.spidma), CLONE_FS|CLONE_FILES|CLONE_SIGHAND|SIGCHLD);
    if(unlikely(amoled_task<0)){
        printk(KERN_WARNING "amoled_threadFun create failed \r\n");
    }
    else{
        printk(KERN_INFO "amoled_threadFun create success \r\n");
    }
#endif*/

    return status;
}
//
static int32_t amoled_release(struct inode *inode, struct file *filp)
{ /*
    Amoled_Struct *as;
    struct spidma_data  *spidma;
    int32_t         status = 0;

    mutex_lock(&device_list_lock);
    as = filp->private_data;
    spidma = as->spi.spidma;
    filp->private_data = NULL;

    // last close?
    spidma->users--;
    if (!spidma->users) {
        int32_t     dofree;

        kfree(spidma->buffer);
        spidma->buffer = NULL;

        // ... after we unbound from the underlying device?
        spin_lock_irq(&spidma->spi_lock);
        dofree = (spidma->spi == NULL);
        spin_unlock_irq(&spidma->spi_lock);

        if (dofree)
            kfree(spidma);
    }
    //
    kfree(as);
    mutex_unlock(&device_list_lock);

    return status;*/
    return 0;
}
/*-------------------------------------------------------------------------*/

static struct class *amoled_class;

/*-------------------------------------------------------------------------*/
const uint8_t loading_20x50_bmp[3000] = {
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF,
    0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF,
    0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF,
    0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF,
    0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF,
    0XFF, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF,
    0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF,
    0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF,
    0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF,
    0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF,
    0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0XFF,
    0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF,
    0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF,
    0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF,
    0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF,
    0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF,
    0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF,
    0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF,
    0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF,
    0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF,
    0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF,
    0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF,
    0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF,
    0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF,
    0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF,
    0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF,
    0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF,
    0XFF, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0XFF, 0XFF,
    0XFF, 0XFF, 0XFF, 0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00};
int32_t amoled_threadFun(void *arg)
{
    DECLARE_WAITQUEUE(wait, current);
    // 初始化LCD
    amoled_work_mode(as->spi.spidma, 0, NULL);
    // logo
    // if(strstr(boot_command_line, "dev=1")) //铁路版
    //     amoled_draw(as->spi.spidma, as->led.xyAddress[0], as->led.xyAddress[1], as->led.xyAddress[2], as->led.xyAddress[3], logo_buffer2);
    // else //标准版
    //     amoled_draw(as->spi.spidma, as->led.xyAddress[0], as->led.xyAddress[1], as->led.xyAddress[2], as->led.xyAddress[3], logo_buffer);
    amoled_draw(as->spi.spidma, 0, as->led.xyAddress[3] - 49, 19, as->led.xyAddress[3], loading_20x50_bmp);
    // amoled_cleer(as->spi.spidma, 0, 0, AMOLED_X_MAX - 1, AMOLED_Y_MAX - 1, 0xFF, 0x00, 0x00);
    return 0;
}

static int32_t __devinit amoled_probe(struct spi_device *spi)
{
    struct spidma_data *spidma;
    int32_t status;
    uint32_t minor;
    int32_t amoled_task;

    printk("--FUN-- amoled_probe\r\n");

    /* Allocate driver data */
    spidma = kzalloc(sizeof(struct spidma_data), GFP_KERNEL);
    if (!spidma)
        return -ENOMEM;

    /* Initialize the driver data */
    spidma->spi = spi;
    spin_lock_init(&spidma->spi_lock);
    mutex_init(&spidma->buf_lock);
    INIT_LIST_HEAD(&spidma->device_entry);

    /* If we can allocate a minor number, hook up this device.
     * Reusing minors is fine so int32_t as udev or mdev is working.
     */
    mutex_lock(&device_list_lock);
    minor = find_first_zero_bit(minors, N_SPI_MINORS);
    if (minor < N_SPI_MINORS)
    {
        struct device *dev;
        spidma->devt = MKDEV(AMOLED_MAJOR, minor);
        dev = device_create(amoled_class, &spi->dev, spidma->devt,
                            spidma, "%s%d.%d", AMOLED_DEV_NAME,
                            spi->master->bus_num, spi->chip_select);
        status = IS_ERR(dev) ? PTR_ERR(dev) : 0;
    }
    else
    {
        dev_dbg(&spi->dev, "no minor number available!\n");
        status = -ENODEV;
    }
    if (status == 0)
    {
        set_bit(minor, minors);
        list_add(&spidma->device_entry, &device_list);
    }
    mutex_unlock(&device_list_lock);

    if (status == 0)
        spi_set_drvdata(spi, spidma);
    else
        kfree(spidma);
    //
    //amoled_framebuffer(spidma);
    dev_info(&spi->dev, "Amoled probe complete!\n");

    if (status)
        return status;

    //amoled init
    printk("amoled init now ...\r\n");
    // 总的参数结构体初始化
    as = (Amoled_Struct *)kmalloc(sizeof(Amoled_Struct), GFP_KERNEL);
    memset(as, 0, sizeof(Amoled_Struct));
    //spidma
    as->spi.spidma = spidma;
    as->spi.spidma->users++;
    as->spi.spidma->buffer = kmalloc(bufsiz, GFP_KERNEL);
    // 默认spi参数
    as->spi.spidma->spi->bits_per_word = AMOLED_DOT_PERB;
    as->spi.spidma->spi->max_speed_hz = AMOLED_MCK; //44000000;
    as->spi.spidma->spi->mode = SPI_MODE_MASK;
    // 默认全屏
    as->led.workMode = 0;
    as->led.xyAddress[0] = 0;
    as->led.xyAddress[1] = 0;
    as->led.xyAddress[2] = AMOLED_X_MAX - 1;
    as->led.xyAddress[3] = AMOLED_Y_MAX - 1;
    as->led.datLen = AMOLED_DOT_MAX;

    //线程启动lcd
    amoled_task = kernel_thread(amoled_threadFun, NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD);

    return status;
}

static int32_t __devexit amoled_remove(struct spi_device *spi)
{
    struct spidma_data *spidma = spi_get_drvdata(spi);

    /* make sure ops on existing fds can abort cleanly */
    spin_lock_irq(&spidma->spi_lock);
    spidma->spi = NULL;
    spi_set_drvdata(spi, NULL);
    spin_unlock_irq(&spidma->spi_lock);

    /* prevent new opens */
    mutex_lock(&device_list_lock);
    list_del(&spidma->device_entry);
    device_destroy(amoled_class, spidma->devt);
    clear_bit(MINOR(spidma->devt), minors);
    if (spidma->users == 0)
        kfree(spidma);
    mutex_unlock(&device_list_lock);

    return 0;
}

static struct spi_driver amoled_spi_driver = {
    .driver = {
        .name = "amoled",
        .owner = THIS_MODULE,
    },
    .probe = amoled_probe,
    .remove = __devexit_p(amoled_remove),

    /* NOTE:  suspend/resume methods are not necessary here.
     * We don't do anything except pass the requests to/from
     * the underlying controller.  The refrigerator handles
     * most issues; the controller driver handles the rest.
     */
};

/*-------------------------------------------------------------------------*/

static int32_t __init amoled_init(void)
{
    int32_t status;

    /* Claim our 256 reserved device numbers.  Then register a class
     * that will key udev/mdev to add/remove /dev nodes.  Last, register
     * the driver which manages those device numbers.
     */
    printk("--FUN-- amoled_init\r\n");

    //gpioc 寄存器起始地址
    gpioc_base = ioremap(0xFFFFF600, SZ_1K); //将物理地映射到虚拟内存地址
    if (!gpioc_base)
        return -ENXIO;

    BUILD_BUG_ON(N_SPI_MINORS > 256);
    status = register_chrdev(AMOLED_MAJOR, "amoled", &amoled_fops);
    if (status < 0)
        return status;

    amoled_class = class_create(THIS_MODULE, "amoled");
    if (IS_ERR(amoled_class))
    {
        unregister_chrdev(AMOLED_MAJOR, amoled_spi_driver.driver.name);
        return PTR_ERR(amoled_class);
    }

    status = spi_register_driver(&amoled_spi_driver);
    if (status < 0)
    {
        class_destroy(amoled_class);
        unregister_chrdev(AMOLED_MAJOR, amoled_spi_driver.driver.name);
        printk("===== Amoled init: spi_register_driver failed ! =====\n");
    }
    else
        printk("===== Amoled init: complete ! =====\n");

#if (TOUCH_ENABLE)
    synaptics_rmi4_init();
#endif

    return status;
}
module_init(amoled_init);

static void __exit amoled_exit(void)
{
    spi_unregister_driver(&amoled_spi_driver);
    class_destroy(amoled_class);
    unregister_chrdev(AMOLED_MAJOR, amoled_spi_driver.driver.name);
    //
    synaptics_rmi4_exit();
}

module_exit(amoled_exit);

MODULE_DESCRIPTION("User mode SPI device interface");
MODULE_LICENSE("GPL");
MODULE_ALIAS("spi:ampled");
