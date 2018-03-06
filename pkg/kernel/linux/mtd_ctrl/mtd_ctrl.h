/****************************************************************************
 *
 * rg/pkg/kernel/linux/mtd_ctrl/mtd_ctrl.h
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

#ifndef _MTD_CTRL_H_
#define _MTD_CTRL_H_

#include <linux/types.h>

#define MTDCTRL_IOC_MAGIC 0xC7

#define MTDCTRL_GET_MTD_NUM _IOR(MTDCTRL_IOC_MAGIC, 1, \
    struct mtdctrl_get_mtd_num_req)

struct mtdctrl_get_mtd_num_req {
    char mtd_name[256];
    int mtd_num;
};

#endif
