/****************************************************************************
 *
 * rg/vendor/huawei/modules/so380000/input_so380000.c
 * 
 * Copyright (C) Jungo LTD 2004
 * 
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General 
 * Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02111-1307, USA.
 *
 * Developed by Jungo LTD.
 * Residential Gateway Software Division
 * www.jungo.com
 * info@jungo.com
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/platform_device.h>

#include <asm/irq.h>
#include <asm/io.h>

#include <bcm_map_part.h>
#include <bcm_intr.h>

#include <util/rg_error.h>

/* GPL code - we can not use KOS API */
typedef struct {
    int code;
    char *str;
} code2str_t;

typedef struct {
    int code;
    int val;
} code2code_t;

static char *code2str(code2str_t *list, int code)
{
    for (; list->code != -1 && list->code != code; list++);

    return list->str;
}

static int code2code(code2code_t *list, int code)
{
    for (; list->code != -1 && list->code != code; list++);

    return list->val;
}

#define BUTTON_TOUCHED_BIT(i) (0x1 << i)
#define BP_GPIO_NUM_MASK  0x00FF

/* Global Data */
static int i2c_transaction_started;

/* Touch button context */
typedef struct {
    struct input_dev *idev;
    struct tasklet_struct tl;
} tb_context_t;

struct touch_key {
    int key;
    enum {
	KEY_RELEASED = 0,
	KEY_PRESSED = 1,
    } state;
    char *name;
};

static struct touch_key key_map[] =
{
    [0] = {KEY_RIGHT, KEY_RELEASED, "Right"},
    [1] = {KEY_LEFT, KEY_RELEASED, "Left"},
    [2] = {KEY_UP, KEY_RELEASED, "Up"},
    [3] = {KEY_ENTER, KEY_RELEASED, "Enter"},
    [4] = {KEY_DOWN, KEY_RELEASED, "Down"},
};

#define IIC_READ  (0x59)
#define IIC_WRITE  (0x58)

#define ACK 0
#define NAK 1

#define CLK_TIME  20
#define SDA 	3 /* GPIO for SDA */
#define SCL 	2 /* GPIO for SCL */
#define ATTN_IRQ INTERRUPT_ID_EXTERNAL_0 /* ATTN is connected to GPIO 24 */

#define I2CDELAY() udelay(CLK_TIME)

/* Hw specific functions */
static inline void bcm63xx_gpio_direction_input(int gpio_num)
{
    GPIO->GPIODir &= ~GPIO_NUM_TO_MASK(gpio_num);
}

static inline void bcm63xx_gpio_direction_output(int gpio_num)
{
    GPIO->GPIODir |= GPIO_NUM_TO_MASK(gpio_num);
}

static inline int bcm63xx_gpio_getval(int gpio_num)
{
    return GPIO->GPIOio & GPIO_NUM_TO_MASK(gpio_num);
}

static inline void bcm63xx_gpio_setval(int gpio_num, int val)
{
    bcm63xx_gpio_direction_output(gpio_num);
    if (val)
	GPIO->GPIOio |= GPIO_NUM_TO_MASK(gpio_num);
    else
	GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(gpio_num);
}

/* Actively drive SCL signal */
static inline void scl_set(int level)
{
    bcm63xx_gpio_setval(SCL, level);
}

/* Actively drive SDA signal */
static inline void sda_set(int level)
{
    bcm63xx_gpio_setval(SDA, level);
}

/* Set SDA line in hi impedance state */
static inline void sda_release(void)
{
    bcm63xx_gpio_direction_input(SDA);
}

static inline int sda_read(void)
{
    return !!bcm63xx_gpio_getval(SDA);
}


/******************* End of Platform specific section ************/

static void arbitration_lost(void)
{
    printk("ARBITRATION_LOST !!!\n");
}
 
static void i2c_start_cond(void)
{
    /* just to be sure */
    scl_set(1); 
    if (!sda_read())
	arbitration_lost();
    /* SCL is high, set SDA from 1 to 0 */
    sda_set(0);
    I2CDELAY();
    scl_set(0);
    I2CDELAY();
    i2c_transaction_started = 1;
}

static void i2c_stop_cond(void)
{
    if (!i2c_transaction_started)
	printk("i2c_stop_cond called in STOP condition\n");
    /* set SDA to 0 */
    sda_set(0);
    I2CDELAY();
    scl_set(1);
    I2CDELAY();

    /* SCL is high, set SDA from 0 to 1 */
    sda_set(1);
    I2CDELAY();
    if (!sda_read())
	arbitration_lost();
    i2c_transaction_started = 0;
}
 
/* Write a bit to I2C bus */
static void i2c_write_bit(int bit)
{
    sda_set(bit);
    I2CDELAY();
    scl_set(1);
    I2CDELAY();
    scl_set(0);
    I2CDELAY();
}

/* Read a bit from I2C bus */
static int i2c_read_bit(void)
{
    int bit;

    /* Let the slave drive data */
    sda_release();
    I2CDELAY();
    scl_set(1);
    I2CDELAY();
    bit = sda_read();
    scl_set(0);
    I2CDELAY();

    return bit;
}
 
/* Write a byte to I2C bus. Return 0 if ack by the slave */
static int i2c_write_u8(u8 byte)
{
    unsigned bit;
    int nak;

    if (!i2c_transaction_started)
	i2c_start_cond();

    for (bit = 0; bit < 8; bit++)
    {
	i2c_write_bit((byte & 0x80) != 0);
	byte <<= 1;
    }
    nak = i2c_read_bit();

    if (nak)
	printk("NAK received\n");

    return nak;
}

static int i2c_write_u16(u16 word)
{
    u8 *buf = (u8 *)&word;

    return i2c_write_u8(buf[0]) || i2c_write_u8(buf[1]);
}

/* Read a byte from I2C bus */
static u8 i2c_read_u8(int ack)
{
    u8 byte = 0;
    unsigned bit;

    for (bit = 0; bit < 8; bit++)
	byte = (byte << 1) | i2c_read_bit();
    i2c_write_bit(ack);
    return byte;
}

static u16 i2c_read_u16(int is_last)
{
    u16 ret;
    u8 *buf = (u8 *)&ret;

    buf[0] = i2c_read_u8(ACK);
    buf[1] = i2c_read_u8(is_last ? NAK : ACK);
    if (is_last)
	i2c_stop_cond();

    return ret;
}

#define write_reg(reg, data) \
    do { \
	i2c_write_u8(IIC_WRITE); \
	i2c_write_u16(reg); \
	i2c_write_u16(data); \
	i2c_stop_cond(); \
    } while (0)

#define write_addr(reg) \
    do { \
	i2c_write_u8(IIC_WRITE); \
	i2c_write_u16(reg); \
    } while (0)

#define read_addr(reg) \
    do { \
	i2c_write_u8(IIC_WRITE); \
	i2c_write_u16(reg); \
	i2c_stop_cond(); \
	i2c_write_u8(IIC_READ); \
    } while (0)

#define LED_RIGHT (1 << 4)
#define LED_LEFT (1 << 3)
#define LED_UP (1 << 2)
#define LED_OK (1 << 1)
#define LED_DOWN (1 << 0)

static void enable_led(int enable, int key)
{
    u16 reg;
    code2code_t key_to_led[] = {
	{ KEY_RIGHT, LED_RIGHT },
	{ KEY_LEFT, LED_LEFT },
	{ KEY_UP, LED_UP },
	{ KEY_ENTER, LED_OK },
	{ KEY_DOWN, LED_DOWN },
	{ -1 },
    };
    u16 led = code2code(key_to_led, key);

    read_addr(0x000e);
    reg = i2c_read_u16(1);

    reg = enable ? reg | led : reg & ~led;

    write_reg(0x000e, reg);
}

static void work_routine(unsigned long data)
{
    u16 reg108, reg109;
    int i;
    int state;
    tb_context_t *ctx = (tb_context_t *)data;
    struct input_dev *idev = ctx->idev;

    /* Clean ATTN line */
    read_addr(0x0108);
    reg108 = i2c_read_u16(0);
    reg109 = i2c_read_u16(1);

    for(i = 0; i < ARRAY_SIZE(key_map); i++)
    {
	if (KEY_RESERVED == key_map[i].key)
	    continue;
	state = (reg109 & BUTTON_TOUCHED_BIT(i)) ? KEY_PRESSED : KEY_RELEASED;
	if(state != key_map[i].state)
	{
	    key_map[i].state = state;
	    input_report_key(idev, key_map[i].key, state);
	    input_sync(idev);
	    enable_led(!(state == KEY_PRESSED), key_map[i].key);
	    rg_error(LINFO, "Touch key \"%s\" button %s", key_map[i].name,
		(reg109 & BUTTON_TOUCHED_BIT(i)) ? "pressed" : "released");
	}
    }

    enable_irq(ATTN_IRQ);
}

static void chip_init(void)
{
    /* Reset of the chip is performed on cfe stage */
#if 0
    write_reg(0x0300, 0x0001); /* Reset command */
    mdelay(30);
#endif

    write_reg(0x0000, 0x0007); /* enable Read and Attention modes */

    /* LEDs section */
    write_addr(0x0022);
    i2c_write_u16(0x001f); /* 0x22 */
    i2c_write_u16(0x0000); /* 0x23 */
    i2c_write_u16(0x1f1f); /* 0x24 */
    i2c_write_u16(0x1f1f); /* 0x25 */
    i2c_write_u16(0x001f); /* 0x26 */
    i2c_stop_cond();

    write_reg(0x000e, 0x001f); /* LEDs ON */

    /* Buttons section */
    write_reg(0x0004, 0x001f); /* Enable buttons */

    write_addr(0x0010); /* Set sensitivity */
    i2c_write_u16(0xaaaa); /* 0x10 */
    i2c_write_u16(0xaaaa); /* 0x11 */
    i2c_write_u16(0x00aa); /* 0x12 */
    i2c_write_u16(0x0000); /* 0x13 */
    i2c_stop_cond();

    /* Reset /ATTN line: clean the interrupt status due to reset */
    read_addr(0x0108);
    i2c_read_u16(0); /* 0x0108 */
    i2c_read_u16(1); /* 0x0109 */
}

static irqreturn_t so380000_isr(int irq, void *data)
{
    tb_context_t *ctx = data;

    disable_irq(irq);
    tasklet_schedule(&ctx->tl);
    return IRQ_HANDLED;
}

static code2str_t registers[] = {
    {0x0000, "interface control"},
    {0x0001, "General Configuration"},
    {0x0004, "Button Enable"},
    {0x000e, "GPIO Control"},
    {0x0010, "Sensor Pin 1/0 Sensitivity"},
    {0x0011, "Sensor Pin 3/2 Sensitivity"},
    {0x0012, "Sensor Pin 5/4 Sensitivity"},
    {0x0013, "Sensor Pin 7/6 Sensitivity"},
    {0x001e, "Button to GPIO Mapping"},
    {0x0022, "LED Enable"},
    {0x0023, "LED Effect Period"},
    {0x0024, "LED Control 1/0"},
    {0x0025, "LED Control 3/2"},
    {0x0026, "LED Control 5/4"},
    {0x0027, "LED Control -/6"},
    {0x0108, "GPIO State"},
    {0x0109, "Button State"},
    {0x010c, "Pressure Values 1/0"},
    {0x010d, "Pressure Values 3/2"},
    {0x010e, "Pressure Values 5/4"},
    {0x010f, "Pressure Values 7/6"},
    {-1, NULL}
};

static int write_proc_so380000(struct file *f, const char *buf,
    unsigned long cnt, void *data)
{
    char input[32];
    unsigned int reg, value, trap;
    int params;

    if ((cnt > 32) || copy_from_user(input, buf, cnt))
        return -EFAULT;

    params = sscanf(input, "%x %x %x", &reg, &value, &trap);

    if (params < 1 || params > 2)
    {
	printk("\n\nusage: echo <reg> <value> > /proc/bcm963xx/so380000\n");
	printk(    "       echo <reg> > /proc/bcm963xx/so380000\n");
        return cnt;
    }

    if (!code2str(registers, reg))
    {
	printk("\nIncorrect register %#x\n", reg);
        return cnt;
    }
    if (params == 2 && value > 0xffff)
    {
	printk("\nIncorrect value %#x\n", value);
        return cnt;
    }

    read_addr(reg);
    printk("\nregister %#x value %#x", reg, i2c_read_u16(1));

    if (params == 2)
    {
	write_reg(reg, value);
	read_addr(reg);
	printk(" -> %#x", i2c_read_u16(1));
    }
    printk("\n");

    return cnt;
}

static int read_proc_so380000(char *page, char **start, off_t off, 
    int count, int *eof, void *data)
{
    int i;
    int len;

    len = sprintf(page, "\n s0380000 registers dump:\n");
    for (i = 0; i < ARRAY_SIZE(registers) - 1; i++)
    {
	read_addr(registers[i].code);
	len += sprintf(page + len, "%#06x\t%-28s %#06x\n", registers[i].code,
	    registers[i].str, i2c_read_u16(1));
    }
    *eof=1;
    return len;
}

static int add_proc_files(void)
{
    struct proc_dir_entry *p1;

    p1 = create_proc_entry("bcm963xx/so380000", 0644, NULL);

    if (p1 == NULL)
        return -1;

    p1->write_proc = write_proc_so380000;
    p1->read_proc = read_proc_so380000;
    p1->data = NULL;
    p1->owner = THIS_MODULE;
    return 0;
}

static int del_proc_files(void)
{
    remove_proc_entry("bcm963xx/so380000", NULL);
    return 0;
}

static int __devinit so380000_probe(struct platform_device *dev)
{
    int i;
    struct input_dev *idev;
    tb_context_t *ctx = kmalloc(sizeof(tb_context_t), GFP_KERNEL);

    idev = input_allocate_device();
    platform_set_drvdata(dev, ctx);

    idev->name = dev->name;
    idev->phys = "so380000/input0";
    idev->cdev.dev = &dev->dev;
    idev->private = NULL;

    idev->id.bustype = BUS_HOST;
    idev->id.vendor = 0x0001;
    idev->id.product = 0x0001;
    idev->id.version = 0x0100;

    set_bit(EV_KEY, idev->evbit);
    for (i = 0; i < ARRAY_SIZE(key_map); i++)
    {
	if (key_map[i].key != KEY_RESERVED)
	    set_bit(key_map[i].key, idev->keybit);
    }
    input_register_device(idev);

    chip_init();

    ctx->idev = idev;
    tasklet_init(&ctx->tl, work_routine, (unsigned long)ctx);
    if (request_irq(ATTN_IRQ, so380000_isr,
	IRQF_SAMPLE_RANDOM | IRQF_TRIGGER_FALLING, "so380000", ctx))
    {
	printk("so380000: Can't allocate irq %d\n", ATTN_IRQ);
	return -EBUSY;
    }

    add_proc_files();

    return 0;
}

static int __devexit so380000_remove(struct platform_device *dev)
{
    tb_context_t *ctx = platform_get_drvdata(dev);

    del_proc_files();

    input_unregister_device(ctx->idev);
    tasklet_disable(&ctx->tl);
    tasklet_kill(&ctx->tl);
    kfree(ctx);
    free_irq(ATTN_IRQ, NULL);
    return 0;
}

static struct platform_driver so380000_driver = {
    .probe = so380000_probe,
    .remove = so380000_remove,
    .driver = { .name = "tb_so380000", },
};

static int __init so380000_init(void)
{
    return platform_driver_register(&so380000_driver);
}

static void __exit so380000_exit(void)
{
    platform_driver_unregister(&so380000_driver);
}

module_init(so380000_init);
module_exit(so380000_exit);

MODULE_LICENSE("GPL");
