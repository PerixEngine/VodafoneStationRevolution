/****************************************************************************
 *
 * rg/vendor/huawei/modules/hx8347b/lcd_hw_hx8347b.c
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

#include <linux/delay.h>
#include <linux/proc_fs.h>

#include "6362_map.h"
#include "boardparms.h"
#include "board.h"
#include "bcmSpiRes.h"

#include <vendor/himax/lcd_hw.h>

#define SLAVEID 0
#define LCD_RESET BP_GPIO_32_AL
#define LCD_FMARK 29
#define LCD_PWM BP_GPIO_28_AH

#define SPI_SPEED (10*1000*1000)
#define SPI_FRAME_SPEED (80*1000*1000)
#define BURST 500

#define CONTRAST_STEPS 3
#define BACKLIGHT_STEPS 11

#define BKLIGHT_MAX_LEVEL 0xa
#define CONTRAST_MAX_LEVEL 0x02

#define LCD_ID 0x70
#define LCD_RS 0x02
#define LCD_READ 0x01
#define LCD_WRITE 0x0
#define DEBUG

extern void fb_hx8347x_lcd_fill(u16 rgb);

void reset_lcd(void)
{
    GPIO->GPIODir &= ~GPIO_NUM_TO_MASK(LCD_FMARK);

    kerSysSetGpio(LCD_RESET, kGpioInactive);
    mdelay(150);

    kerSysSetGpio(LCD_RESET, kGpioActive);
    mdelay(10);
    kerSysSetGpio(LCD_RESET, kGpioInactive);
    mdelay(150);
}

static void lcd_power(int on)
{
    kerSysSetGpio(LCD_PWM, on ? kGpioActive : kGpioInactive);
}

static void lcd_sync(void)
{
    while (!(GPIO->GPIOio & GPIO_NUM_TO_MASK(LCD_FMARK)));
}

static void putreg16(u16 val, u8 cmd)
{
    u8 w[4], *tmp;

    w[0] = cmd;

    tmp = (u8 *)&val;
    w[1] = tmp[0];
    w[2] = tmp[1];
    /* XXX replace with spi_sync */
    if (BcmSpiSyncTrans(w, NULL, 0, 3, 1, SLAVEID))
	printk("LCD SPI ERROR\n");
}

static u16 Get_LCD_16B_REG(u16 reg)
{
    unsigned char w[4];
    struct {
	u8 dummy[2];
	u16 val;
    } r;

    putreg16(reg, LCD_ID);
    w[0] = LCD_ID | LCD_READ | LCD_RS;
    if (BcmSpiSyncTrans(w, (u8 *)&r.dummy[1], 1, 3, 1, SLAVEID))
	printk("LCD SPI ERROR\n");

    return r.val;
}

static void Set_LCD_16B_REG(u8 reg, u16 val)
{
    putreg16(reg, LCD_ID);
    putreg16(val, LCD_ID | LCD_RS);
#ifdef DEBUG
    /* Verify writing */
    if (reg != 0x22)
    {
	u16 read = Get_LCD_16B_REG(reg);
	if (read != val)
	    printk("reg %#x %#x->%#x\n", reg, val, read);
    }
#endif
}

static void lcd_hw_spi_reserve(void)
{
    BcmSpiReserveSlave2(1, SLAVEID, SPI_SPEED, SPI_MODE_DEFAULT,
	SPI_CONTROLLER_STATE_DEFAULT/*SPI_CONTROLLER_STATE_ASYNC_CLOCK*/);
}

static void lcd_hw_spi_release(void)
{
    BcmSpiReleaseSlave(1, SLAVEID);
}

#ifdef DEBUG
static void transfer_buf(u16 *trbuf, int len)
{
    u8 *start = (u8 *)trbuf;

    start++;
    start[0] = LCD_ID | LCD_WRITE | LCD_RS ;

    if (BcmSpiSyncTrans(start, NULL, 0, len-1, 1, SLAVEID))
	printk("LCD SPI ERROR\n");
}
#endif

static int is_lcd_configured(void)
{
    return Get_LCD_16B_REG(0x00A5) == 0x5373;
}

static void lcd_config(void)
{
    //************* Start Initial Sequence **********//
    Set_LCD_16B_REG(0x00A5, 0x5373);
    Set_LCD_16B_REG(0x00E4, 0x0100);
    Set_LCD_16B_REG(0x00E5, 0x1010);
    Set_LCD_16B_REG(0x00E8, 0x0080);
    Set_LCD_16B_REG(0x00ED, 0x0014);
    Set_LCD_16B_REG(0x0001, 0x0100); // set SS and SM bit
    Set_LCD_16B_REG(0x0002, 0x0700); // set 1 line inversion
    Set_LCD_16B_REG(0x0003, 0x1008 /*0x1030*/); // set GRAM write direction and BGR=1.
    Set_LCD_16B_REG(0x0004, 0x0000); // Resize register
    Set_LCD_16B_REG(0x0008, 0x0202); // set the back porch and front porch
    Set_LCD_16B_REG(0x0009, 0x0000); // set non-display area refresh cycle ISC[3:0
    Set_LCD_16B_REG(0x000A, 0x0008 /*0x0000*/); // FMARK function
    Set_LCD_16B_REG(0x000C, 0x0000 /*0x0110*/); // RGB setting, RM=1,DM-01, RIM=00(262k)
    Set_LCD_16B_REG(0x000D, 0x0000); // Frame marker Position
    Set_LCD_16B_REG(0x000F, 0x0000 /*0x0002*/); // RGB polarity, EPL=1
    //*************Power On sequence ****************//
    Set_LCD_16B_REG(0x0010, 0x0000); // SAP, BT[3:0], AP, DSTB, SLP, STB
    Set_LCD_16B_REG(0x0011, 0x0007); // DC1[2:0], DC0[2:0], VC[2:0]
    Set_LCD_16B_REG(0x0012, 0x0000); // VREG1OUT voltage
    Set_LCD_16B_REG(0x0013, 0x0000); // VDV[4:0] for VCOM amplitude
    Set_LCD_16B_REG(0x0007, 0x0000);
    mdelay(50);
    Set_LCD_16B_REG(0x0010, 0x1490 /*0x1690*/); // SAP, BT[3:0], AP, DSTB, SLP, STB
    Set_LCD_16B_REG(0x0011, 0x0447 /*0x0227*/); // Set DC1[2:0], DC0[2:0], VC[2:0]
    mdelay(50);
    Set_LCD_16B_REG(0x0012, 0x0208 /*0x009C*/); // External reference voltage= Vci;
    mdelay(50);
    Set_LCD_16B_REG(0x0013, 0x0A00 /*0x1800*/); // VDV[4:0] for VCOM amplitude
    Set_LCD_16B_REG(0x0029, 0x0020 /*0x0008*/); // VCM[5:0] for VCOMH
    Set_LCD_16B_REG(0x002B, 0x000A); /* Set Frame Rate to 60Hz to optimize
				      * crosstalk */
    mdelay(50);
    Set_LCD_16B_REG(0x0020, 0x0000); // GRAM horizontal Address
    Set_LCD_16B_REG(0x0021, 0x0000); // GRAM Vertical Address
    mdelay(50);
    // ----------- Adjust the Gamma Curve ----------//
    Set_LCD_16B_REG(0x0030, 0x0000);
    Set_LCD_16B_REG(0x0031, 0x0507);
    Set_LCD_16B_REG(0x0032, 0x0103);
    Set_LCD_16B_REG(0x0035, 0x0404);
    Set_LCD_16B_REG(0x0036, 0x1200);
    Set_LCD_16B_REG(0x0037, 0x0406);
    Set_LCD_16B_REG(0x0038, 0x0002);
    Set_LCD_16B_REG(0x0039, 0x0707);
    Set_LCD_16B_REG(0x003C, 0x0201);
    Set_LCD_16B_REG(0x003D, 0x000C);
    //------------------ Set GRAM area ---------------//
    Set_LCD_16B_REG(0x0050, 0x0000);
    Set_LCD_16B_REG(0x0051, 0x00EF);
    Set_LCD_16B_REG(0x0052, 0x0000);
    Set_LCD_16B_REG(0x0053, 0x013F);
    // Horizontal GRAM Start Address
    // Horizontal GRAM End Address
    // Vertical GRAM Start Address
    // Vertical GRAM Start Address
    // Gate Scan Line
    Set_LCD_16B_REG(0x0061, 0x0001); // NDL,VLE, REV
    Set_LCD_16B_REG(0x006A, 0x0000); // set scrolling line
    //-------------- Partial Display Control ---------//
    Set_LCD_16B_REG(0x0080, 0x0000);
    Set_LCD_16B_REG(0x0081, 0x0000);
    Set_LCD_16B_REG(0x0082, 0x0000);
    Set_LCD_16B_REG(0x0083, 0x0000);
    Set_LCD_16B_REG(0x0084, 0x0000);
    Set_LCD_16B_REG(0x0085, 0x0000);
    //-------------- Panel Control -------------------//
    Set_LCD_16B_REG(0x0090, 0x001e /*0x0010*/);
    Set_LCD_16B_REG(0x0092, 0x0000 /*0x0600*/);
    Set_LCD_16B_REG(0x0007, 0x0133);// 262K color and display ON

}

#if 0
XXX to remove . Use as a reference.
static void hx8347b_sleep(int wake_up)
{
    if (wake_up)
    {
	// Exit Sleep mode Setting
	Set_LCD_16B_REG(0x0010, 0x0080); // SAP, BT[3:0], AP, DSTB, SLP
	Set_LCD_16B_REG(0x0007, 0x0001);
	mdelay (50);
	Set_LCD_16B_REG(0x0010, 0x1690); // SAP, BT[3:0], AP, DSTB, SLP, STB
	mdelay (50);
	Set_LCD_16B_REG(0x0007, 0x0133); // 262K color and display ON
    }
    else
    {
	// Enter Sleep mode Setting
	Set_LCD_16B_REG(0x0007, 0x0131);
	mdelay(10);
	Set_LCD_16B_REG(0x0007, 0x0130);
	mdelay(10);
	Set_LCD_16B_REG(0x0007, 0x0000);
	mdelay(50);
	Set_LCD_16B_REG(0x0010, 0x0082);
	// Set D1=0, D0=1
	// Set D1=0, D0=0
	// display OFF
	// SAP, BT[3:0], APE, AP, DSTB, SLP
    }
}

void clear_screen(hx8347b_dev_t *dev)
{
    struct spi_message *msg;
    struct spi_transfer *t;

    memset(trbuf, 0x0f, BURST);

    msg = create_msg((u8 *)trbuf, 0, 240*320*2);


    list_for_each_entry(t, &msg->transfers, transfer_list)
    {
	t->tx_buf = trbuf;
    }

    Set_LCD_16B_REG(0x20, 0x0);
    Set_LCD_16B_REG(0x21, 0x0);

    /* Once set R22h addres and fill whole the memory */
    putreg16(0x22, LCD_ID);

    spi_sync(dev->spi, msg);
    spi_message_free(msg);

#if 0
    /* Some override in last loop does not cause any problem */
    for (i = 0; i < 240*320; i += BURST/2)
	transfer_buf(trbuf, BURST);

    for (i = 0; i < 0x12c00; i++)
	putreg16(0, LCD_ID | LCD_RS);
#endif

}

void show_red_screen(void)
{
    int i;

    Set_LCD_16B_REG(0x20, 0x0);
    Set_LCD_16B_REG(0x21, 0x0);

    /* Once set R22h addres and fill whole the memory */
    putreg16(0x22, LCD_ID);
    for (i = 0; i < 240*320; i++)
	putreg16(0x07e0, LCD_ID | LCD_RS);

#if 0
    /* Alternative way - write each pixel explicitly */
    for (i = 0; i < 0x12c0; i++)
	Set_LCD_16B_REG(0x22, 0xf800);
#endif
}
#endif

static u16 value_reg_12[CONTRAST_STEPS] = {
    0x020a, 0x0209, 0x0208
};

static u16 value_reg_13[BACKLIGHT_STEPS] = {
    0x1600, 0x1500, 0x1400, 0x0F00, 0x0E00, 0x0D00, 0x0C00, 0x0B00, 0x0A00,
    0x0900, 0x0800
};

static u16 value_reg_29[CONTRAST_STEPS][BACKLIGHT_STEPS] = {
    { 0x002E, 0x002E, 0x002B, 0x0029, 0x0025, 0x0025, 0x0023, 0x0022, 0x0022,
	0x001B, 0x001A },
    { 0x0030, 0x002B, 0x002A, 0x002a, 0x0025, 0x0023, 0x0020, 0x0020, 0x001B,
	0x001B, 0x0019 },
    { 0x0030, 0x002A, 0x002A, 0x002a, 0x0023, 0x0021, 0x001E, 0x001E, 0x0020,
	0x001A, 0x0016 }
};

static int get_index(u16 *arr, int max_size, u16 val)
{
    int i;

    for(i = 0; i < max_size && arr[i] != val; i++);

    return i != max_size ? i : -1;
}

static void update_reg(int backlight_index, int contrast_index)
{
    if (contrast_index >= 0)
    {
	backlight_index = get_index(value_reg_13, BACKLIGHT_STEPS,
	    Get_LCD_16B_REG(0x13));
	if (backlight_index < 0 || contrast_index >= CONTRAST_STEPS)
	{
	    printk("Invalid params\n");
	    return;
	}
	Set_LCD_16B_REG(0x12, value_reg_12[contrast_index]);
	Set_LCD_16B_REG(0x29, value_reg_29[contrast_index][backlight_index]);
    }
    else if (backlight_index >= 0)
    {
	contrast_index = get_index(value_reg_12, CONTRAST_STEPS,
	    Get_LCD_16B_REG(0x12));
	if (contrast_index < 0 || backlight_index >= BACKLIGHT_STEPS)
	{
	    printk("Invalid params\n");
	    return;
	}
	Set_LCD_16B_REG(0x13, value_reg_13[backlight_index]);
	Set_LCD_16B_REG(0x29, value_reg_29[contrast_index][backlight_index]);
    }
}

static void hx8347b_backlight(int backlight)
{
    update_reg(backlight, -1);
}

static void hx8347b_contrast(int contrast)
{
    update_reg(-1, contrast);
}

static void lcd_init(void)
{
    /* If LCD was configured by RGLOADER - no need reset in again: this will
     * cause screen blinking */
    if (is_lcd_configured())
    {
	printk("LCD (ID code %#x) was already configured\n",
	    Get_LCD_16B_REG(0x0));
    }
    else
    {
	reset_lcd();
	printk("LCD ID code %#x\n", Get_LCD_16B_REG(0x0));
	lcd_config();
    }
}

static void lcd_prepare(void)
{
    Set_LCD_16B_REG(0x20, 0x0);
    Set_LCD_16B_REG(0x21, 0x0);

    /* Once set R22h addres and fill whole the memory */
    putreg16(0x22, LCD_ID);

    /* Wait for frame begin */
    lcd_sync();
}

/* ---------------------------------- proc entry -------------------------- */

static void show_dump(void)
{
    static u16 regs[] = {0x00, 0x01, 0x02, 0x03, 0x05, 0x07, 0x08, 0x09, 0x0a,
	0x0c, 0x0d, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x29, 0x2b, 0x30, 0x31, 0x32,
	0x35, 0x36, 0x37, 0x38, 0x39, 0x3c, 0x3d, 0x50, 0x51, 0x52, 0x53, 0x60,
	0x61, 0x6a, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x90, 0x92, 0x97, 0x98,
	0xa2, 0xa3, 0xa4, 0xa5, 0xb2, 0xb4, 0xb6, 0xbf, 0xda, 0xdb, 0xdc, 0xf3,
	0xf4, 0xf5};
    int i;

    printk("\n LCD registers dump:\n");
    for (i = 0; i < ARRAY_SIZE(regs); i++)
	printk("%#04x\t%#06x\n", regs[i], Get_LCD_16B_REG(regs[i]));
}

#ifdef DEBUG
static struct delayed_work demo_work;
static int demo_started;

static void show_demo_work(struct work_struct *w)
{
    static int color;
    u16 colors[4] = {0xC0CA, 0xBEEF, 0xC01D, 0xCAFE};

    fb_hx8347x_lcd_fill(colors[color++&0x3]);

    if (demo_started)
	schedule_delayed_work(&demo_work, msecs_to_jiffies(300));
}

static void show_demo(int start)
{
    if (start)
    {
	if (demo_started)
	    return;

	INIT_DELAYED_WORK(&demo_work, show_demo_work);
	schedule_delayed_work(&demo_work, msecs_to_jiffies(100));
	demo_started = 1;
    }
    else
	demo_started = 0;
}

static void fill_zone(unsigned int start, unsigned int len, u16 color)
{
    int i, pos;
    u16 *trbuf;

    Set_LCD_16B_REG(0x20, start & 0x00ff);
    Set_LCD_16B_REG(0x21, (start >> 8) & 0x1ff);

    putreg16(0x22, LCD_ID);
    pos = 1;

    trbuf = kmalloc(BURST, GFP_KERNEL);

    for (i = 0; i < len; i++)
    {
	trbuf[pos++] = color;
	if (pos > BURST/2)
	{
	    transfer_buf(trbuf, pos*2);
	    pos = 1;
	}
    }

    if (pos != 1)
	transfer_buf(trbuf, pos*2);

    kfree(trbuf);
}
#endif

int proc_set_lcd(struct file *f, const char *buf, unsigned long cnt, void *data)
{
    char input[32];
    unsigned int reg, value, color, trap;
    int params;

    if ((cnt > 32) || (copy_from_user(input, buf, cnt) != 0))
        return -EFAULT;

    if (strstr(input, "dump"))
    {
	show_dump();
	return cnt;
    }
#ifdef DEBUG
    if (sscanf(input, "demo %d", &value))
    {
	show_demo(value);
	return cnt;
    }
#endif

    params = sscanf(input, "%x %x %x %x", &reg, &value, &color, &trap);

    if (params < 1 || params > 3)
    {
	printk("\n\nusage: echo <reg> <value> > /proc/bcm963xx/LCD\n");
	printk(    "       echo <reg> > /proc/bcm963xx/LCD\n");
	printk(    "       echo dump > /proc/bcm963xx/LCD\n");
#ifdef DEBUG
	printk(    "       echo <start> <len> <color> > /proc/bcm963xx/LCD\n");
	printk(    "       echo demo <0|1> > /proc/bcm963xx/LCD\n");
#endif
        return cnt;
    }

    if (params == 3)
    {
#ifdef DEBUG
	fill_zone(reg, value, color);
#endif
	return cnt;
    }

    if (reg > 0xf5)
    {
	printk("\nIncorrect register %#x\n", reg);
        return cnt;
    }
    if (params == 2 && value > 0xffff)
    {
	printk("\nIncorrect value %#x\n", value);
        return cnt;
    }

    printk("\nregister %#x value %#x", reg, Get_LCD_16B_REG(reg));
    if (params == 2)
    {
	Set_LCD_16B_REG(reg, value);
	printk(" -> %#x", Get_LCD_16B_REG(reg));
    }
    printk("\n");

    return cnt;
}

static int add_proc_files(void)
{
    struct proc_dir_entry *p1;

    p1 = create_proc_entry("bcm963xx/LCD", 0644, NULL);

    if (p1 == NULL)
        return -1;

    p1->write_proc = proc_set_lcd;
    p1->data = NULL;
    p1->owner = THIS_MODULE;
    return 0;
}

static int del_proc_files(void)
{
    remove_proc_entry("bcm963xx/LCD", NULL);
    return 0;
}

void lcd_hx8347b_init(void)
{
    lcd_hw_ops.frame_prefix = LCD_ID|LCD_WRITE|LCD_RS;
    lcd_hw_ops.spi_driver_name = "spi:hx8347b-lcd";
    lcd_hw_ops.burst = BURST;
    lcd_hw_ops.spi_frame_speed = SPI_FRAME_SPEED;
    lcd_hw_ops.contrast_max_level = CONTRAST_MAX_LEVEL;
    lcd_hw_ops.bklight_max_level = BKLIGHT_MAX_LEVEL;
    lcd_hw_ops.lcd_hw_init = lcd_init;
    lcd_hw_ops.lcd_contrast_set = hx8347b_contrast;
    lcd_hw_ops.lcd_bklight_set = hx8347b_backlight;
    lcd_hw_ops.lcd_power = lcd_power;
    lcd_hw_ops.lcd_hw_spi_reserve = lcd_hw_spi_reserve;
    lcd_hw_ops.lcd_hw_spi_release = lcd_hw_spi_release;
    lcd_hw_ops.lcd_prepare = lcd_prepare;
    lcd_hw_ops.add_proc_files = add_proc_files;
    lcd_hw_ops.del_proc_files = del_proc_files;
}

