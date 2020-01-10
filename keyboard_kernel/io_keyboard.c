#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/slab.h>

//#include <mach/regs-gpio.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/err.h>

#include <linux/timer.h>
#include <linux/jiffies.h>


//---------- driver param ---------- 传参初始化

#define IO_KEYBOARD_IO_MAX 64

static int io[IO_KEYBOARD_IO_MAX] = {0};
static int ioNum = 0;
module_param_array(io, int, &ioNum, 0664);
// MODULE_PARM_DESC(io, "This is a array.");

static int type[IO_KEYBOARD_IO_MAX] = {0};
static int typeNum = 0;
module_param_array(type, int, &typeNum, 0664);
// MODULE_PARM_DESC(type, "This is a array.");

static int delay = 10000;
module_param(delay, int, 0664);

static int debug = 0;
module_param(debug, int, 0664);
static struct task_struct *key_task;
bool keythrd_run = false;

//---------- driver param ----------

#define IO_KEYBOARD "io_keyboard"

typedef struct _io_keyboard_key{
	unsigned int gpio;	    //对应gpio口
	unsigned int irq;	    //对应中断
	int n_key;			    //键值
    unsigned char keybuf;   //扫描缓冲区，保存一段时间内的扫描值
    unsigned char keybuf_bak;
}io_keyboard_key;

struct io_keyboard_kbd{
	io_keyboard_key keys[IO_KEYBOARD_IO_MAX];
	//struct timer_list key_timer;//按键去抖定时器
	unsigned int key_status; 	//按键状态
    bool timer_run;
    unsigned long timer_cnt;
	struct input_dev *input;
};

struct io_keyboard_kbd *p_io_keyboard_kbd;

struct io_keyboard_kbd *get_kbd(void)
{
	// printk("get_kbd p_io_keyboard_kbd=%x\n", (unsigned int)p_io_keyboard_kbd);
	return p_io_keyboard_kbd;
}

struct timer_list keytimer;
//定时器开机后大概5分钟才能起来，原因未知
static void keycheck_func(unsigned long data)
{
    int i;
    // printk("keycheck_func\r\n");
    struct io_keyboard_kbd *p_kbd = (struct io_keyboard_kbd *)data;
    if(p_kbd->timer_run == false && p_kbd->timer_cnt++ > 400)
    {
        p_kbd->timer_run = true;
        printk("timer keycheck_func run success\r\n");
    }
    else if(p_kbd->timer_cnt < 400)
    {
        mod_timer(&keytimer, 400);//单位：50us
        return;
    }
    
    for(i = 0;i < ioNum; i++)
    {
        //缓冲区左移一位，并将当前扫描值移入最低位
        p_kbd->keys[i].keybuf = (p_kbd->keys[i].keybuf<<1) | gpio_get_value_cansleep(p_kbd->keys[i].gpio); 
        if ((p_kbd->keys[i].keybuf & 0x0F) == 0x00)
        { 
            if(p_kbd->keys[i].keybuf_bak != (p_kbd->keys[i].keybuf & 0x0F))
            {
                if(debug)
                    printk("timer keycheck n_key=%d, key_state=%d\n", p_kbd->keys[i].n_key, 1);
                p_kbd->keys[i].keybuf_bak = (p_kbd->keys[i].keybuf & 0x0F);
                input_report_key(p_kbd->input, p_kbd->keys[i].n_key, 1);//1表示按下
                input_sync(p_kbd->input);
            }
        }
        else if (p_kbd->keys[i].keybuf == 0x0F)
        { 
            if(p_kbd->keys[i].keybuf_bak != (p_kbd->keys[i].keybuf & 0x0F))
            {
                if(debug)
                    printk("timer keycheck n_key=%d, key_state=%d\n", p_kbd->keys[i].n_key, 0);
                p_kbd->keys[i].keybuf_bak = (p_kbd->keys[i].keybuf & 0x0F);
                input_report_key(p_kbd->input, p_kbd->keys[i].n_key, 0);//0表示弹起
                input_sync(p_kbd->input);
            }
        }
        else {} //其它情况则说明按键状态尚未稳定，则不对key值进行更新
    }
    mod_timer(&keytimer, 400);//单位：50us

}

static int __init keytimer_init(unsigned long data)
{
    setup_timer(&keytimer, keycheck_func, data);
    #if 0
	init_timer(&keytimer);
	keytimer.data = data;
	keytimer.expires = jiffies + HZ;
	keytimer.function = keycheck_func;
	#endif
    keytimer.expires = 1000;
    add_timer(&keytimer);
    printk("keytimer_init\r\n");
    return 0;
}
static void __exit keytimer_exit(void)
{
    del_timer(&keytimer);
}

void set_kbd(struct io_keyboard_kbd *p_kbd)
{
	p_io_keyboard_kbd = p_kbd;
	// printk("set_kbd p_kbd=%x, p_io_keyboard_kbd=%x\n", (unsigned int)p_kbd, (unsigned int)p_io_keyboard_kbd);
}

static irqreturn_t io_keyboard_kbd_handler(int irq, void *p_date)
{
	unsigned int n_key = 0;
	struct io_keyboard_kbd *p_kbd = p_date;
	unsigned int key_state = 0;
	int i, j;
  
    for(i = 0; i < ioNum; i++)
    {
        if( irq == p_kbd->keys[i].irq )
        {
			key_state = (gpio_get_value_cansleep(p_kbd->keys[i].gpio) ? 0 : 1);
            n_key = p_kbd->keys[i].n_key;
            break;
        }
    }
	
	//按键去抖定时器
    for(j = 0; j < delay; j++)
        nop();

    if(key_state == (gpio_get_value_cansleep(p_kbd->keys[i].gpio) ? 0 : 1))
    {
        // if(debug)
        //     printk("io_keyboard n_key=%d, key_state=%d\n", n_key, key_state);
        // input_report_key(p_kbd->input, n_key, key_state);//1表示按下
        // input_sync(p_kbd->input);
    }

    return IRQ_HANDLED;
}
//定时器启动前先用线程检测按键
static int keycheck_kernel_thread(void *data)
{
    int i;
    printk("keycheck_kernel_thread start\r\n");
    struct io_keyboard_kbd *p_kbd = (struct io_keyboard_kbd *)data;
    while(keythrd_run && p_kbd->timer_run == false)
    {
        set_current_state(TASK_UNINTERRUPTIBLE);
        if(kthread_should_stop()) break;
        for(i = 0;i < ioNum; i++)
        {
            //缓冲区左移一位，并将当前扫描值移入最低位
            p_kbd->keys[i].keybuf = (p_kbd->keys[i].keybuf<<1) | gpio_get_value_cansleep(p_kbd->keys[i].gpio); 
            //线程延时不准，检测3位
            if ((p_kbd->keys[i].keybuf & 0x07) == 0x00)
            { 
                if(p_kbd->keys[i].keybuf_bak != (p_kbd->keys[i].keybuf & 0x07))
                {
                    if(debug)
                        printk("thread keycheck n_key=%d, key_state=%d\n", p_kbd->keys[i].n_key, 1);
                    p_kbd->keys[i].keybuf_bak = (p_kbd->keys[i].keybuf & 0x07);
                    input_report_key(p_kbd->input, p_kbd->keys[i].n_key, 1);//1表示按下
                    input_sync(p_kbd->input);
                }
            }
            else if ((p_kbd->keys[i].keybuf & 0x07) == 0x07)
            { 
                if(p_kbd->keys[i].keybuf_bak != (p_kbd->keys[i].keybuf & 0x07))
                {
                    if(debug)
                        printk("thread keycheck n_key=%d, key_state=%d\n", p_kbd->keys[i].n_key, 0);
                    p_kbd->keys[i].keybuf_bak = (p_kbd->keys[i].keybuf & 0x07);
                    input_report_key(p_kbd->input, p_kbd->keys[i].n_key, 0);//0表示弹起
                    input_sync(p_kbd->input);
                }
            }
            else {} //其它情况则说明按键状态尚未稳定，则不对key值进行更新
        }
        schedule_timeout(1);//不定，实际延时约10ms
    }
    printk("keycheck_kernel_thread break\n");
    kthread_stop(key_task);

    return 0;
}

static void kbd_free_irqs(void)
{
    struct io_keyboard_kbd *p_kbd = get_kbd();
    int i;

    // printk("kbd_free_irqs p_kbd=%x\n", (unsigned int)p_kbd);
    
    for(i = 0; i < ioNum; i++)
        free_irq(p_kbd->keys[i].irq, p_kbd);
}

static int kbd_req_irqs(void)
{
    int n_ret;
    int i;
    struct io_keyboard_kbd *p_kbd = get_kbd();

    // printk("kbd_req_irqs p_kbd=%x\n", (unsigned int)p_kbd);

    for(i = 0; i < ioNum; i++)
    {
        n_ret = request_irq(p_kbd->keys[i].irq, io_keyboard_kbd_handler, IRQ_TYPE_EDGE_BOTH, IO_KEYBOARD, p_kbd);
        if(n_ret)
        {
            printk("%d: could not register interrupt\n", p_kbd->keys[i].irq);
            goto fail;
        }
    }

    return n_ret;

fail:
    //因为上面申请失败的那个没有成功,所以也不要释放
    for(i--; i >= 0; i--)
    {
        disable_irq(p_kbd->keys[i].irq);
        free_irq(p_kbd->keys[i].irq, p_kbd);
    }

    return n_ret;
}

static void io_keyboard_param_load(struct io_keyboard_kbd *p_kbd)
{
	struct input_dev *input_dev = p_kbd->input;

    // ----- key install like that , 4 step -----
    
    // p_kbd->keys[0].gpio = EXYNOS4_GPX3(2);
    // p_kbd->keys[1].gpio = EXYNOS4_GPX3(3);
    // p_kbd->keys[2].gpio = EXYNOS4_GPX3(4);
    // p_kbd->keys[3].gpio = EXYNOS4_GPX3(5);

    // p_kbd->keys[0].irq = gpio_to_irq(p_kbd->keys[0].gpio);
    // p_kbd->keys[1].irq = gpio_to_irq(p_kbd->keys[1].gpio);
    // p_kbd->keys[2].irq = gpio_to_irq(p_kbd->keys[2].gpio);
    // p_kbd->keys[3].irq = gpio_to_irq(p_kbd->keys[3].gpio);

    // p_kbd->keys[0].n_key = KEY_0;
    // p_kbd->keys[1].n_key = KEY_1;
    // p_kbd->keys[2].n_key = KEY_2;
    // p_kbd->keys[3].n_key = KEY_3;

    // __set_bit(EV_KEY, input_dev->evbit);
    // __set_bit(KEY_0, input_dev->keybit);
    // __set_bit(KEY_1, input_dev->keybit);
    // __set_bit(KEY_2, input_dev->keybit);
    // __set_bit(KEY_3, input_dev->keybit);
	
	int i;
	for(i = 0; i < ioNum; i++)
	{
        type[i] += KEY_RESERVED;
        //
		p_kbd->keys[i].gpio = io[i];
		p_kbd->keys[i].irq = gpio_to_irq(io[i]);
		p_kbd->keys[i].n_key = type[i];
		__set_bit(type[i], input_dev->keybit);
        //
        printk("  io[%d] / %d - type[%d] / %d\n", i, io[i], i, type[i]);
	}

	__set_bit(EV_KEY, input_dev->evbit);
    input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REP);
}

static int io_keyboard_probe(struct platform_device *pdev)
{
    int i;
    int err = -ENOMEM;
    struct io_keyboard_kbd *p_io_keyboard = NULL;
    struct input_dev *input_dev = NULL;

    // printk("io_keyboard probe\n");

    p_io_keyboard = kmalloc(sizeof(struct io_keyboard_kbd), GFP_KERNEL);
    if( !p_io_keyboard )
    {
        printk("io_keyboard_probe kmalloc error!\n");
        return err;
    }

    input_dev = input_allocate_device();
    if (!input_dev)
    {
        printk("io_keyboard_probe input_allocate_device error!\n");
        goto fail;
    }
	input_dev->name = pdev->name;
    input_dev->phys = IO_KEYBOARD"/input0";
    input_dev->id.bustype = BUS_HOST;
    input_dev->dev.parent = &pdev->dev;
    input_dev->id.vendor = 0x0001;
    input_dev->id.product = 0x0001;
    input_dev->id.version = 0x0100;

    p_io_keyboard->input = input_dev;
    io_keyboard_param_load(p_io_keyboard);

    err = input_register_device(input_dev);
    if( err )
    {
        printk("io_keyboard_probe input_register_device error!\n");
        goto fail_allocate;
    }

    set_kbd(p_io_keyboard);
    platform_set_drvdata(pdev, p_io_keyboard);

    err = kbd_req_irqs();
    if( err )
    {
        printk("io_keyboard_probe kbd_req_irqs error!\n");
        goto fail_register;
    }

    printk("io_keyboard probe sucess!\n");
    keythrd_run = true;
    key_task=kthread_create(keycheck_kernel_thread, (void *)p_io_keyboard, "key_task");
    if(IS_ERR(key_task)){
        printk("Unable to start kernel thread. ");
        err = PTR_ERR(key_task);
        key_task = NULL;
        goto fail_register;
    }
    wake_up_process(key_task);
    // p_kbd = get_kbd();
    p_io_keyboard->timer_run = false;
    p_io_keyboard->timer_cnt = 0;
    for(i=0; i<ioNum; i++)
    {
        p_io_keyboard->keys[i].keybuf_bak = 0xFF;
    }
    keytimer_init((unsigned long) p_io_keyboard);
    

    return 0;

fail_register:
    input_unregister_device(input_dev);
    goto fail;
fail_allocate:
    input_free_device(input_dev);
fail:
    kfree(p_io_keyboard);

    return err;
}

static int io_keyboard_remove(struct platform_device *pdev)
{
    struct io_keyboard_kbd *p_io_keyboard = platform_get_drvdata(pdev);

    printk("io_keyboard remove\n");

    kbd_free_irqs();
    input_unregister_device(p_io_keyboard->input);
    kfree(p_io_keyboard);

    printk("io_keyboard remove sucess!\n");

    return 0;
}

static void io_keyboard_release(struct device *dev)
{
    dev = dev;
}

static struct platform_driver io_keyboard_device_driver = {
    .probe = io_keyboard_probe,
    .remove = io_keyboard_remove,
    .driver = {
        .name = IO_KEYBOARD,
        .owner = THIS_MODULE,
    }
};

static struct platform_device io_keyboard_device_kbd = {
    .name = IO_KEYBOARD,
    .id = -1,
    .dev = {
        .release = io_keyboard_release,
    }
};

static int __init io_keyboard_init(void)
{
    int n_ret;
    // struct io_keyboard_kbd *p_kbd = NULL;
    
    printk("example: insmod ./io_keyboard.ko io=1,2,3 type=10,11,12\n");

    printk("io_keyboard init\n");

    n_ret = platform_driver_register(&io_keyboard_device_driver);

    // printk("io_keyboard_init 1 n_ret=%d jiffies=%lu,HZ=%d\n", n_ret, jiffies, HZ);
    if( n_ret )
        return n_ret;

    n_ret = platform_device_register(&io_keyboard_device_kbd);

    // printk("io_keyboard_init 2 n_ret=%d\n", n_ret);
    if( n_ret )
        goto fail;
    return n_ret;

fail:
    platform_driver_unregister(&io_keyboard_device_driver);

    return n_ret;
}

static void __exit io_keyboard_exit(void)
{
    printk("io_keyboard exit\n");
    keytimer_exit();
    keythrd_run = false;
    // if(key_task){
    //     kthread_stop(key_task);
    //     key_task = NULL;
    // }
    platform_device_unregister(&io_keyboard_device_kbd);
    platform_driver_unregister(&io_keyboard_device_driver);
}

module_init(io_keyboard_init);
module_exit(io_keyboard_exit);

MODULE_DESCRIPTION("io keyboard driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:io keyboard driver");
