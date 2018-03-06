JMK_ROOT=../../../../../../../..
include $(JMK_ROOT)/envir.mak

# File: bcmdrivers/opensource/net/xtmrt/Makefile
#
# Makefile for the BCM63xx bcmxtmrt driver.
#

obj-y := bcmxtmrt.o bcmxtmrtbond.o

EXTRA_CFLAGS += -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMBOARDPARMS_PATH)/$(BRCM_BOARD) \
-I$(INC_BRCMSHARED_PRIV_PATH)/$(BRCM_BOARD) -I$(INC_BRCMDRIVER_PRIV_PATH)/$(BRCM_BOARD) -DDPERF_MON_BONDING_US -DDPTMBOND_US_PRIO_TRAFFIC_SPLIT
EXTRA_CFLAGS += -I$(BRCMDRIVERS_DIR)/opensource/net/enet/shared
EXTRA_CFLAGS += -I$(BRCMDRIVERS_DIR)/opensource/net/xtmrt/impl$(CONFIG_BCM_XTMRT_IMPL)
EXTRA_CFLAGS += -I$(BRCMDRIVERS_DIR)/broadcom/char/fap/impl$(CONFIG_BCM_FAP_IMPL)
EXTRA_CFLAGS += -I$(BRCMDRIVERS_DIR)/broadcom/char/fap/impl$(CONFIG_BCM_FAP_IMPL)/4ke
ifeq "$(BRCM_PHY_LOOPBACK)" "y"
EXTRA_CFLAGS += -DPHY_LOOPBACK
endif


JMK_INTERNAL_HEADERS+=bcmxtmrtimpl.h bcmxtmrtbond.h

JMK_MOD_TARGET+=$(obj-y) $(obj-m)
JMK_MOD_2_STAT+=$(obj-y)
JMK_RAMDISK_MODULES_FILES+=$(obj-m)
JMK_MOD_CFLAGS+=$(EXTRA_CFLAGS)

JMK_O_OBJS_bcmxtmrt.o=bcmxtmrt.o
JMK_O_OBJS_bcmxtmrtbond.o=bcmxtmrtbond.o

include $(JMKE_MK)
