/****************************************************************************
 *
 * rg/pkg/kernel/linux/mtd_ctrl/mtd_ctrl.c
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
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/mtd/mtd.h>
#include <asm/uaccess.h>
#include "mtd_ctrl.h"

static long mtd_ctrl_ioctl(struct file *file, unsigned int cmd,
    unsigned long arg)
{
    void __user *argp = (void __user *)arg;
    struct mtd_info *mtd;
    int err = 0;

    switch (cmd)
    {
    case MTDCTRL_GET_MTD_NUM:
	{
	    struct mtdctrl_get_mtd_num_req r;

	    if (copy_from_user(&r, argp, sizeof(r)))
	    {
		err = -EFAULT;
		break;
	    }

	    mtd = get_mtd_device_nm(r.mtd_name);
	    if (IS_ERR(mtd))
	    {
		err = PTR_ERR(mtd);
		break;
	    }

	    err = put_user(mtd->index, (__user int *)
		(&((struct mtdctrl_get_mtd_num_req *)argp)->mtd_num));

	    put_mtd_device(mtd);
	    break;
	}

    default:
	err = -ENOTTY;
	break;
    }

    return err;
}

static struct file_operations mtd_ctrl_cdev_ops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = mtd_ctrl_ioctl,
};

static struct miscdevice mtd_ctrl_cdev = {
    .minor = MTD_CTRL_MINOR,
    .name = "mtd_ctrl",
    .fops = &mtd_ctrl_cdev_ops,
};

static int __init mtd_ctrl_init(void)
{
    int ret;

    if ((ret = misc_register(&mtd_ctrl_cdev)))
	printk(KERN_ERR "%s: Can't register chardev (%d)\n", __func__, ret);

    return ret;
}

static void __exit mtd_ctrl_exit(void)
{
    misc_deregister(&mtd_ctrl_cdev);    
}

module_init(mtd_ctrl_init);
module_exit(mtd_ctrl_exit);

MODULE_ALIAS_MISCDEV(MTD_CTRL_MINOR);
MODULE_LICENSE("GPL");
