#ifndef _MISC_H_
#define _MISC_H_

#define RG_BOOT_MAGIC 0x59eeb001

#ifdef CONFIG_MIPS
/* Weaken the platform's 'prom_init' function (and alias it as 'plat_prom_init'),
 * since in RG we have our own 'prom_init' that needs to call platform's
 * original 'prom_init'.
 */
#define WEAK_PROM_INIT() \
  void __init __attribute__ ((weak)) prom_init(void); \
  void __init __attribute__ ((alias("prom_init"))) plat_prom_init(void); \
  void __init prom_init(void)
#endif

#endif
