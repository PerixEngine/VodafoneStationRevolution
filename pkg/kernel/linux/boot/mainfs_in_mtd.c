/****************************************************************************
 *
 * rg/pkg/kernel/linux/boot/mainfs_in_mtd.c
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

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/err.h>
#include <linux/workqueue.h>
#include <linux/moduleparam.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/ubi.h>
#include <linux/slab.h>
#include <asm/io.h>

static struct mtd_partition mainfs_part = {
    .name = CONFIG_RG_MAINFS_MTDPART_NAME,
    .mask_flags = MTD_WRITEABLE, /* mainfs is RO */
};

typedef struct {
    struct work_struct work;
    char *mtd_name;
} mainfs_in_mtd_work_t;

static void mainfs_mtdparts(struct work_struct *work)
{
    mainfs_in_mtd_work_t *w = container_of(work, mainfs_in_mtd_work_t, work);
    struct mtd_info *mtd;

    if (!mainfs_part.size)
    {
	printk("MainFS in MTD: No partition was set in the cmdline\n");
	return;
    }

    mtd = get_mtd_device_nm(w->mtd_name);
    if (IS_ERR(mtd))
    {
	printk("Failed to find MTD device %s\n", w->mtd_name);
	return;
    }

#if defined(CONFIG_RG_OS_LINUX_3X)
    if (mtd_add_partition(mtd, mainfs_part.name, mainfs_part.offset,
	mainfs_part.size) < 0)
#elif defined(CONFIG_RG_OS_LINUX_26)	
    if (add_mtd_partitions(mtd, &mainfs_part, 1) < 0)
#endif
    {
	printk("Failed to create %s MTD partition\n", mainfs_part.name);
	put_mtd_device(mtd);
	return;
    }
    /* At this point, the mainfs partition is created on top of the
     * CONFIG_RG_PERM_STORAGE_DEFAULT_PARTITION partition.
     * We keep our reference of mtd so that the UBI volume is kept open.
     */
    kfree(w);
}

static void defer(struct mtd_info *mtd, char *mtd_name, 
    void (*func)(struct work_struct *work))
{
    mainfs_in_mtd_work_t *w;

    if (strcmp(mtd->name, mtd_name))
	return;

    if (!(w = kmalloc(sizeof(*w), GFP_KERNEL)))
	return;

    INIT_WORK(&w->work, func);
    w->mtd_name = mtd_name;
    schedule_work(&w->work);
}

static void mainfs_in_mtd_notify_add(struct mtd_info *mtd)
{
    /* Since we cannot directly call code that creates MTD devices from
     * an MTD notifier, we defer the work to a workqueue.
     */
    defer(mtd, CONFIG_RG_PERM_STORAGE_DEFAULT_PARTITION, mainfs_mtdparts);
}

static struct mtd_notifier mainfs_in_mtd = {
    .add = mainfs_in_mtd_notify_add,
};

static int __init mainfs_in_mtd_init(void)
{
    register_mtd_user(&mainfs_in_mtd);
    printk("Registered MAINFS in MTD notifier\n");
    return 0;
}

module_init(mainfs_in_mtd_init);

static int __init mainfs_in_mtd_part_parse(const char *str,
    struct kernel_param *kp)
{
    char *t;

    if (!str)
	return -EINVAL;
    mainfs_part.offset = simple_strtoul(str, &t, 0);
    if (t == str || *t != ',')
	return -EINVAL;
    mainfs_part.size = simple_strtoul(t + 1, NULL, 0);
    return 0;
}

module_param_call(part, mainfs_in_mtd_part_parse, NULL, NULL, 000);
MODULE_PARM_DESC(part, "MainFS in MTD partition definition.\n"
		       "Parameter format: part=offset,size.\n");
