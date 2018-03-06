JMK_ROOT=../../../../../../..
include $(JMK_ROOT)/envir.mak

obj-$(CONFIG_BCM_PCI) += fixup-bcm63xx.o pci-bcm63xx.o ops-bcm63xx.o

EXTRA_CFLAGS += -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD)

JMK_MOD_TARGET+=bcm_pci_mod.o
JMK_MOD_2_STAT+=bcm_pci_mod.o
JMK_O_OBJS+=$(obj-y)
JMK_MOD_CFLAGS+=$(EXTRA_CFLAGS)

include $(JMKE_MK)	
