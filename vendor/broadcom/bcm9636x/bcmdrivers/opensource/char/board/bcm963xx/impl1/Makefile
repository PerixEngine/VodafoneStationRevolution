JMK_ROOT=../../../../../../../../..
include $(JMK_ROOT)/envir.mak

# Makefile for the bcm963xx board drivers
#


obj-y := board.o bcm63xx_flash.o bcm63xx_led.o bcm63xx_gpio.o


ifeq ($(strip $(BRCM_CHIP)),6816)
obj-y += spidevices.o
endif

ifeq ($(strip $(BRCM_CHIP)),6818)

ifneq ($(strip $(BUILD_6802_MOCA)),)
    obj-y += bbsi/bbsi.o    
    obj-y += bbsi/bbsiExecSeq.o   
    obj-y += moca/6802/ont.o   
    
else
    obj-y += spidevices.o
endif

endif

ifeq ($(strip $(BRCM_CHIP)),63268)
obj-y += spidevices.o
endif

ifneq ($(strip $(CONFIG_BCM_CPLD1)),)
    obj-y += bcmcpld1.o
	EXTRA_CFLAGS += -DCONFIG_BCM_CPLD1
endif

EXTRA_CFLAGS += -I. -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD)

ifdef CONFIG_ADB_MCU_LEDS
    obj-y += kmcu_api.o
    EXTRA_CFLAGS += -I$(JMKE_BUILDDIR)/pkg/include/vendor/broadcom/bcm9636x
endif

EXTRA_CFLAGS += -Werror -Wfatal-errors
EXTRA_CFLAGS += -g

ifneq ($(strip $(CONFIG_BCM_WLAN)),)
	EXTRA_CFLAGS += -DWIRELESS
endif

JMK_MOD_TARGET+=bcm_board_mod.o
JMK_MOD_2_STAT+=bcm_board_mod.o
JMK_O_OBJS+=$(obj-y)
JMK_MOD_CFLAGS+=$(EXTRA_CFLAGS)


JMK_RAMDISK_LAST_TASKS+=board_chardev

board_chardev:
	$(JMKE_MKNOD) $(JMKE_RAMDISK_DEV_DIR)/brcmboard c 206 0

include $(JMKE_MK)
