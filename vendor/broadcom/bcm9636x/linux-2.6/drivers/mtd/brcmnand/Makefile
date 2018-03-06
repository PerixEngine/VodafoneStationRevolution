JMK_ROOT=../../../../../../..
include $(JMK_ROOT)/envir.mak

#
# Makefile for the Broadcom NAND MTD
#

obj-$(CONFIG_MTD_BRCMNAND)		+=  bcm63xx-nand.o brcmnand_base.o brcmnand_bbt.o
## obj-$(CONFIG_MTD_BRCMNAND_EDU)		+= edu.o
obj-$(CONFIG_MTD_BRCMNAND_CORRECTABLE_ERR_HANDLING)	+= brcmnand_cet.o
EXTRA_CFLAGS	+= -I $(TOPDIR)/include/asm-generic -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD)


JMK_INTERNAL_HEADERS+=brcmnand_priv.h

brcmnand-$(CONFIG_MTD_BRCMNAND)+=brcmnand_mod.o
JMK_O_OBJS_brcmnand_mod.o+=$(obj-y) $(obj-m)

JMK_MOD_TARGET+=$(brcmnand-y) $(brcmnand-m)
JMK_MOD_2_STAT+=$(brcmnand-y)
JMK_RAMDISK_MODULES_FILES+=$(brcmnand-m)
JMK_MOD_CFLAGS+=$(EXTRA_CFLAGS)

include $(JMKE_MK)
