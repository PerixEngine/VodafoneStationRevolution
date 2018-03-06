JMK_ROOT=../../../../
include $(JMK_ROOT)/envir.mak

JMK_SUBDIRS+=$(if $(CONFIG_PCI),arch/mips/pci)
JMK_SUBDIRS+=$(if $(CONFIG_MTD),drivers/mtd)
JMK_SUBDIRS+=$(if $(CONFIG_NET),net/core)

KERNEL_DIR=$(JMKE_BUILDDIR)/os/linux-2.6

KER_LINKS=$(addprefix $(KERNEL_DIR)/,\
  arch/mips/bcm963xx \
  include/linux/blog.h \
  include/linux/blog_net.h \
  include/linux/blog_rule.h \
  include/linux/gpio.h \
  include/linux/nbuff.h \
  include/linux/nbuff_types.h \
  include/linux/iqos.h \
  include/linux/gbpm.h \
  include/linux/bcm_assert.h \
  include/linux/bcm_assert_locks.h \
  include/linux/bcm_colors.h \
  include/linux/bcm_log.h \
  include/linux/bcm_log_mod.h \
  include/linux/bcm_realtime.h \
  include/linux/bcm_skb_defines.h \
  include/linux/bcm_swversion.h \
  include/linux/bcm_tstamp.h \
  include/linux/mtd \
  include/mtd \
  include/asm-generic)

KER_ASMLINKS=$(addprefix $(KERNEL_DIR)/,\
  include/asm/mach-bcm963xx \
  include/asm/bounce.h \
  include/asm/pmonapi.h)

JMK_ARCHCONFIG_FIRST_TASKS+=$(KER_LINKS) $(KER_ASMLINKS)

$(KER_LINKS):
	$(MKDIR) $(dir $@)
	$(JMKE_CP_LN) $(@:$(KERNEL_DIR)/%=$(JMKE_PWD_SRC)/%) $@

$(KER_ASMLINKS):
	$(MKDIR) $(dir $@)	
	$(JMKE_CP_LN) $(@:$(KERNEL_DIR)/%=$(JMKE_PWD_SRC)/arch/mips/%) $@

include $(JMKE_MK)
