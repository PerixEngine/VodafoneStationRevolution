JMK_ROOT=../../../../../../../../..
include $(JMK_ROOT)/envir.mak

# File: bcmdrivers/opensource/char/pktdma
#
# Makefile for the BCM63xx pktdma driver

obj-$(CONFIG_BCM_PKTDMA) += bcmPktDmaEthIudma.o
obj-$(CONFIG_BCM_PKTDMA) += bcmPktDmaBds.o

ifneq ($(strip $(CONFIG_BCM_XTMCFG))$(strip $(CONFIG_BCM_XTMCFG_MODULE)),)
obj-$(CONFIG_BCM_PKTDMA) += bcmPktDmaXtmIudma.o
endif

ifneq ($(strip $(CONFIG_BCM_FAP)),)
obj-$(CONFIG_BCM_PKTDMA) += bcmPktDmaEthDqm.o
obj-$(CONFIG_BCM_PKTDMA) += bcmPktDma.o
ifneq ($(strip $(CONFIG_BCM_XTMCFG))$(strip $(CONFIG_BCM_XTMCFG_MODULE)),)
obj-$(CONFIG_BCM_PKTDMA) += bcmPktDmaXtmDqm.o
endif
endif

EXTRA_CFLAGS += -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD)
#  EXTRA_CFLAGS += -I$(INC_BRCMDRIVER_PRIV_PATH)/$(BRCM_BOARD) 
EXTRA_CFLAGS += -I$(BRCMDRIVERS_DIR)/opensource/net/enet/shared
EXTRA_CFLAGS += -I$(BRCMDRIVERS_DIR)/broadcom/char/fap/impl1
EXTRA_CFLAGS += -I$(BRCMDRIVERS_DIR)/broadcom/char/fap/impl1/4ke
ifeq ($(CONFIG_BCM_XTMRT_IMPL),1)
EXTRA_CFLAGS += -I$(BRCMDRIVERS_DIR)/opensource/net/xtmrt/impl3
else
EXTRA_CFLAGS += -I$(BRCMDRIVERS_DIR)/opensource/net/xtmrt/impl$(CONFIG_BCM_XTMRT_IMPL)
endif
#  EXTRA_CFLAGS += -I$(INC_BRCMSHARED_PRIV_PATH)/$(BRCM_BOARD)
EXTRA_CFLAGS += -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD)

EXTRA_CFLAGS += -Werror -Wall -Wfatal-errors


bcm_pktdma-$(CONFIG_BCM_PKTDMA)=bcm_pktdma_mod.o
JMK_O_OBJS_bcm_pktdma_mod.o+=$(obj-y) $(obj-m)

JMK_MOD_TARGET+=$(bcm_pktdma-y) $(bcm_pktdma-m)
JMK_MOD_2_STAT+=$(bcm_pktdma-y)
JMK_RAMDISK_MODULES_FILES+=$(bcm_pktdma-m)
JMK_MOD_CFLAGS+=$(EXTRA_CFLAGS)

include $(JMKE_MK)
