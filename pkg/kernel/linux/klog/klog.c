/****************************************************************************
 *
 * rg/pkg/kernel/linux/klog/klog.c * 
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
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/reboot.h>

#include "klog.h"
#include "klog_console.h"
#include "console_tap.h"

klog_storage_ops_t *klog_storage_ops;
EXPORT_SYMBOL(klog_storage_ops);

static int klog_panic_notify(struct notifier_block *self, unsigned long cmd,
    void *ptr)
{
    klog_storage_ops->klog_storage_mark_crash();

    return 0;
}

static struct notifier_block klog_panic_notifier = {
    .notifier_call = klog_panic_notify,
};

int klog_storage_ops_register(klog_storage_ops_t *ops)
{
    if (klog_storage_ops)
    {
	printk(KERN_ALERT "klog backend already loaded\n");
	return -EEXIST;
    }

    klog_storage_ops = ops;

    if (klog_console_init() ||
	atomic_notifier_chain_register(&panic_notifier_list,
	&klog_panic_notifier))
    {
	printk(KERN_ALERT "klog initialization failed\n");
	return -EPERM;
    }

    console_tap_set(klog_storage_ops->klog_storage_write);

    printk(KERN_ALERT "klog initialization succeeded\n");

    return 0;
}
EXPORT_SYMBOL(klog_storage_ops_register);

int klog_storage_ops_unregister(void)
{
    int rc = 0;

    console_tap_set(NULL);

    rc |= atomic_notifier_chain_unregister(&panic_notifier_list,
	&klog_panic_notifier); 

    rc |= klog_console_uninit();

    if (rc)
	printk(KERN_ALERT "klog uninitialization failed\n");

    klog_storage_ops = NULL;

    return rc;
}
EXPORT_SYMBOL(klog_storage_ops_unregister);

int __init klog_init(void)
{
    printk(KERN_ALERT "klog loaded\n");

    return 0;
}

static void __exit klog_exit(void)
{
    atomic_notifier_chain_unregister(&panic_notifier_list,
	&klog_panic_notifier); 
}

#ifdef MODULE
module_init(klog_init);
module_exit(klog_exit);
#endif

MODULE_LICENSE("GPL");
