JMK_ROOT=../../../../../../..
include $(JMK_ROOT)/envir.mak

ifeq ($(CONFIG_MIPS_BRCM),y)

# Linux
obj-y		+= boardparms.o

ifneq ($(strip $(VOXXXLOAD)),)
obj-y		+= boardparms_voice.o
endif

EXTRA_CFLAGS += -DCONFIG_BCM9$(BRCM_CHIP) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD)

endif

JMK_MOD_TARGET+=boardparms_mod.o
JMK_MOD_2_STAT+=boardparms_mod.o
JMK_O_OBJS+=$(obj-y)
JMK_MOD_CFLAGS+=$(EXTRA_CFLAGS)

include $(JMKE_MK)
