/****************************************************************************
 *
 * rg/pkg/kernel/linux/klog/klog_console.c * 
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
#include <linux/console.h>

#include "klog.h"

static void klog_console_write(struct console *console, const char *buf,
    unsigned len)
{
    klog_storage_ops->klog_storage_write(buf, len);
}

static struct console klog_console = {
    .name = "klog",
    .write = klog_console_write,
    .flags = CON_PRINTBUFFER | CON_ENABLED,
    .index = -1,
};

int klog_console_uninit(void)
{
    unregister_console(&klog_console);
    return 0;
}

int klog_console_init(void)
{
    register_console(&klog_console);
    return 0;
}
