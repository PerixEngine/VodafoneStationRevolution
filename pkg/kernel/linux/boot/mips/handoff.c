/****************************************************************************
 *
 * rg/pkg/kernel/linux/boot/mips/handoff.c
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

#include <linux/types.h>
#include <boot/rg_boot.h>

extern void flush_cache_all(void);
extern int puts(const char *s);
extern void puthex(unsigned long n);
extern char cmdline[];

static void kernel_args_prep(ulong *ka0, ulong *ka1, ulong *ka2, ulong *ka3)
{
    /* Keep these global, their addresses are passed to the kernel */
    static const char *argv[2];
    static const char *envp[1];
    static const char empty_str[1];

    /* Pass the cmdline in the commonly used argc,argv,envp format.
     * Openrg kernel provides its own prom_init implementation to handle
     * the passed args.
     */

    /* argv[0] is usually not used by the argc,argv,envp format parsers.
     * nevertheless pass a valid pointer (to an empty string), just in case. */
    argv[0] = empty_str;
    /* pass the cmdline. it is expected to be in bss/data */
    argv[1] = cmdline;

    *ka0 = 2; /* argc */
    *ka1 = (ulong)argv;
    *ka2 = (ulong)envp;
    *ka3 = RG_BOOT_MAGIC;

#ifdef DEBUG
    puts("ka0="); puthex(*ka0); puts(" ");
    puts("ka1="); puthex(*ka1); puts(" ");
    puts("ka2="); puthex(*ka2); puts(" ");
    puts("ka3="); puthex(*ka3); puts("\n");
#endif
}

void do_boot_kernel(void *kernel_entry)
{
    register ulong (*volatile kernel)(ulong, ulong, ulong, ulong);
    ulong ka0, ka1, ka2, ka3;

    kernel_args_prep(&ka0, &ka1, &ka2, &ka3);
    kernel = kernel_entry;

#if (defined(CONFIG_CPU_MIPS32) || defined(CONFIG_CPU_MIPS64) || \
    defined(CONFIG_CPU_RLX5281))
    flush_cache_all();
#endif

    kernel(ka0, ka1, ka2, ka3);
}
