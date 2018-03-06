/****************************************************************************
 *
 * rg/pkg/kernel/linux/klog/console_tap.c * 
 * 
 * Modifications by Jungo Ltd. Copyright (C) 2008 Jungo Ltd.  
 * All Rights reserved. 
 * 
 * This program is free software; 
 * you can redistribute it and/or modify it under 
 * the terms of the GNU General Public License version 2 as published by 
 * the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty 
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
 * See the GNU General Public License v2 for more details.
 * 
 * You should have received a copy of the GNU General Public License 
 * along with this program. 
 * If not, write to the Free Software Foundation, 
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA, 
 * or contact Jungo Ltd. 
 * at http://www.jungo.com/openrg/opensource_ack.html
 * 
 * 
 * 
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <asm/uaccess.h>

#include "console_tap.h"

static void (*console_tap_fn)(const char *buffer, unsigned int length);
static struct file *console_file, *console_tap_file;
static struct tty_driver *console_tap_driver;
static struct mutex console_tap_lock;

static void redirect_enable(int enable)
{
    struct file *filp;

    filp = enable ? console_tap_file : console_file;

#if defined(CONFIG_RG_OS_LINUX_3X)
    filp->f_op->unlocked_ioctl(filp, TIOCCONS, 0);
#elif defined(CONFIG_RG_OS_LINUX_26)
    filp->f_op->ioctl(filp->f_path.dentry->d_inode, filp, TIOCCONS, 0);
#endif
}

static int console_tap_open(struct tty_struct *tty, struct file *filp)
{
    return 0;
}

static int console_tap_write(struct tty_struct *tty,
    const unsigned char *buf, int count)
{
    mm_segment_t old_fs;
    ssize_t res;

    /* There is a trick here:
     * 1. By default write calls intended for /dev/console are redirected to
     *    /dev/console_tap - and arrive here
     * 2. The data buffer is passed to (*console_tap_fn)()
     * 3. The redirection is removed
     * 4. The data buffer is written to /dev/console. This time, the 'write'
     *    will not be redirected and will be sent the real console driver
     * 5. The redirection is restored
     */

    mutex_lock(&console_tap_lock);
    if (console_tap_fn)
	console_tap_fn(buf, count);

    redirect_enable(0);
    old_fs = get_fs();
    set_fs(get_ds());
    /* The cast to a user pointer is valid due to the set_fs() */
    res = vfs_write(console_file, (const char __user *)buf, count,
	&console_file->f_pos);
    set_fs(old_fs);
    redirect_enable(1);
    mutex_unlock(&console_tap_lock);
    return res;
}

static int console_tap_write_room(struct tty_struct *tty)
{
    return 4096;
}

static const struct tty_operations console_tap_ops = {
    .open = console_tap_open,
    .write = console_tap_write,
    .write_room = console_tap_write_room,
};

void console_tap_set(void (*tap_fn)(const char *buffer,
    unsigned int length))
{
    if (tap_fn)
    {
	console_tap_fn = tap_fn;
	redirect_enable(1);
	printk("console_tap: tap function is set\n");
    }
    else
    {
	printk("console_tap: tap function is unset\n");
	mutex_lock(&console_tap_lock);
	redirect_enable(0);
	console_tap_fn = NULL;
	mutex_unlock(&console_tap_lock);
    }
}
EXPORT_SYMBOL(console_tap_set);

static void console_tap_exit(void)
{
    if (console_file)
	filp_close(console_file, NULL);
    if (console_tap_file)
	filp_close(console_tap_file, NULL);
    tty_unregister_driver(console_tap_driver);
    put_tty_driver(console_tap_driver);
}

static __init int console_tap_init(void)
{
    struct tty_driver *driver;

    if (!(driver = alloc_tty_driver(1)))
    {
	printk("console_tap: failed to alloc tty driver\n");
	return -1;
    }

    driver->owner = THIS_MODULE;
    driver->driver_name = "console_tap_driver";
    driver->name = "console_tap";
    driver->major = TTYAUX_MAJOR;
    driver->minor_start = 53;
    driver->type = TTY_DRIVER_TYPE_CONSOLE;
    driver->init_termios = tty_std_termios;
    driver->flags = TTY_DRIVER_REAL_RAW;
    tty_set_operations(driver, &console_tap_ops);

    if (tty_register_driver(driver))
    {
	printk("console_tap: failed to register tty driver\n");
	put_tty_driver(driver);
	return -1;
    }

    console_tap_driver = driver;

    console_file = filp_open("/dev/console", O_WRONLY, 0);
    if (IS_ERR(console_file))
    {
	printk("console_tap: failed to open console device\n");
	console_file = NULL;
	goto Error;
    }

    console_tap_file = filp_open("/dev/console_tap", O_WRONLY, 0);
    if (IS_ERR(console_tap_file))
    {
	printk("console_tap: failed to open console tap device\n");
	console_tap_file = NULL;
	goto Error;
    }

    mutex_init(&console_tap_lock);
    printk("console_tap: loaded\n");
    return 0;

Error:
    console_tap_exit();
    return -1;
}

module_init(console_tap_init);
module_exit(console_tap_exit);
