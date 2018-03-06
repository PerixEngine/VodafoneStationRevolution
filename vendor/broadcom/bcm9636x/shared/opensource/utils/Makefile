JMK_ROOT=../../../../../..
include $(JMK_ROOT)/envir.mak

ifeq ($(CONFIG_MIPS_BRCM),y)

# Linux
obj-y		+= shared_utils.o


EXTRA_CFLAGS += -DCONFIG_BCM9$(BRCM_CHIP) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD)  -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD)

-include $(TOPDIR)/Rules.make

endif

JMK_MOD_TARGET+=bcmutils_mod.o
JMK_MOD_2_STAT+=bcmutils_mod.o
JMK_O_OBJS+=$(obj-y)
JMK_MOD_CFLAGS+=$(EXTRA_CFLAGS)

include $(JMKE_MK)
