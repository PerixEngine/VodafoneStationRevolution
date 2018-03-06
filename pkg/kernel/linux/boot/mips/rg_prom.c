/****************************************************************************
 *
 * rg/pkg/kernel/linux/boot/mips/rg_prom.c
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
#include <linux/module.h>
#include <linux/init.h>
#include <asm/bootinfo.h>
#include <boot/rg_boot.h>
#include <boot/rg_boot_user.h>

u8 rg_board_type;
EXPORT_SYMBOL(rg_board_type);
u8 rg_manufacturing_mode = 0;
EXPORT_SYMBOL(rg_manufacturing_mode);

static int __init board_type_setup(char *str)
{
    rg_board_type = simple_strtoul(str, NULL, 0);
    return 1;
}
__setup("boardtype=", board_type_setup);

static int __init manufacturing_mode_setup(char *str)
{
    rg_manufacturing_mode = 1;
    return 1;
}
__setup("manufacturing_mode", manufacturing_mode_setup);

void __init prom_init(void)
{
    extern void __init plat_prom_init(void);
    int i;
    int argc = fw_arg0;
    const char **argv = (const char **)fw_arg1;
    int is_rgboot = fw_arg3 == RG_BOOT_MAGIC;

    /* Currently, in mips, if rgboot image format is used, we do not use
     * any of the original bootloader passed arguments (actually, rgboot does
     * not even store them nor pass them to the kernel).
     * Hence, there's no way to pass proper fw args to platform's prom_init.
     * Pass NULL instead, explicitly state they're no good.
     * Yep, each platform's prom_init should be altered to support zeroed
     * args.
     */
    if (is_rgboot)
	fw_arg0 = fw_arg1 = fw_arg2 = fw_arg3 = 0;

    plat_prom_init();

    /* Not booted by rgboot? Done */
    if (!is_rgboot)
    {
	/* CFE passes silent serial flag in the first argument to RGLoader */
	if (fw_arg0 & RG_BOOT_SILENT_SERIAL)
	    strcat(arcs_cmdline, " silent_serial");

	return;
    }

    /* Build the cmdline from the arguments passed by rgboot.
     * Concat them after existing arcs_cmdline content.
     */
    strlcat(arcs_cmdline, " ", sizeof(arcs_cmdline));
    for (i = 1; i < argc; i++)
    {
	strlcat(arcs_cmdline, argv[i], sizeof(arcs_cmdline));
	strlcat(arcs_cmdline, " ", sizeof(arcs_cmdline));
    }
}
