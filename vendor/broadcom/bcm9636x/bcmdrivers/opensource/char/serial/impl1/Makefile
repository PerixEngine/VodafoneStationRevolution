JMK_ROOT=../../../../../../../..
include $(JMK_ROOT)/envir.mak

# File: bcmdrivers/opensource/char/serial
#
# Makefile for the BCM63xx serial/console driver

obj-$(CONFIG_BCM_SERIAL) += bcm63xx_cons_mod.o

EXTRA_CFLAGS += -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD)
EXTRA_CFLAGS += -Werror -Wfatal-errors

JMK_MOD_TARGET+=$(obj-y) $(obj-m)
JMK_MOD_2_STAT+=$(obj-y)
JMK_RAMDISK_MODULES_FILES+=$(obj-m)
JMK_MOD_CFLAGS+=$(EXTRA_CFLAGS)

$(foreach o,$(JMK_MOD_TARGET), \
  $(eval JMK_O_OBJS_$o+=$(o:%_mod.o=%.o)))

include $(JMKE_MK)
