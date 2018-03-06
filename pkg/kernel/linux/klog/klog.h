/****************************************************************************
 *
 * rg/pkg/kernel/linux/klog/klog.h * 
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

#ifndef _KLOG_H_
#define _KLOG_H_

#ifdef __KERNEL__
#include <linux/time.h>
#endif

typedef enum {
    KLOG_NO_CRASH,
    KLOG_CRASH,
    KLOG_ERROR,
} klog_status_t;

typedef struct klog_storage_data_t {
    char *buffer;
    unsigned int size;
    unsigned int num_reboots;
    struct timeval timestamp;
    klog_status_t status;
} klog_storage_data_t;

#ifdef __KERNEL__
typedef struct {
    int (*klog_storage_init)(void);
    void (*klog_storage_write)(const char *buffer, unsigned int length);
    void (*klog_storage_mark_crash)(void);
    int (*klog_storage_save_cur)(void);
    klog_storage_data_t *(*klog_storage_last_crash_data_get)(void);
    klog_storage_data_t *(*klog_storage_cur_buf_data_get)(void);
    void (*klog_storage_clean)(void);
    void (*klog_storage_buffer_free)(int is_crash);
} klog_storage_ops_t;

extern klog_storage_ops_t *klog_storage_ops;

int klog_storage_ops_register(klog_storage_ops_t *ops);
int klog_storage_ops_unregister(void);
#endif

#endif
