/****************************************************************************
 *
 * rg/pkg/kernel/linux/krgldr/krgldr.h * 
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

#ifndef _KRGLDR_H_
#define _KRGLDR_H_

#define MAINFS_PERMST_MAGIC 0xbeeff00d
#define KRGLDR_IOCTL_BASE 0xF0

#include <asm/ioctl.h>

typedef struct { 
    unsigned int offset;
    unsigned int size;
    int is_physmap_flash_boot;
    unsigned long rg_param0;
    unsigned long rg_param1;
} krgldr_param_t;

typedef struct { 
    unsigned int pa_addr;
    void *data;
    unsigned int size;
} krgldr_io_write_t;

#define RGLDR_RAM_WRITE _IOW(KRGLDR_IOCTL_BASE, 0, krgldr_io_write_t)
#define RGLDR_REBOOT _IOW(KRGLDR_IOCTL_BASE, 1, krgldr_param_t)

#endif
