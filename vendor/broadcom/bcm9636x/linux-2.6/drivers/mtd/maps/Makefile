JMK_ROOT=../../../../../../..
include $(JMK_ROOT)/envir.mak

obj-$(CONFIG_MTD_BCM963XX) += bcm963xx_mtd_mod.o
EXTRA_CFLAGS += -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD)

JMK_MOD_TARGET+=$(obj-y) $(obj-m)
JMK_MOD_2_STAT+=$(obj-y)
JMK_RAMDISK_MODULES_FILES+=$(obj-m)
JMK_MOD_CFLAGS+=$(EXTRA_CFLAGS)

JMK_O_OBJS_bcm963xx_mtd_mod.o+=bcm963xx.o

include $(JMKE_MK)	
