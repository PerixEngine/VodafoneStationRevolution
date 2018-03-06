JMK_ROOT=../../../..
include $(JMK_ROOT)/envir.mak

JMK_SUBDIRS+=opensource/include/bcm963xx
JMK_SUBDIRS+=$(if $(CONFIG_BCM_BOARD),opensource/char/board/$(BRCM_BOARD)/impl1)
JMK_SUBDIRS+=$(if $(CONFIG_BCM_SERIAL),opensource/char/serial/impl1)
JMK_SUBDIRS+=$(if $(CONFIG_BCM_PKTDMA),opensource/net/lib/pktdma/impl1)
JMK_SUBDIRS+=$(if $(CONFIG_BCM_XTMRT),opensource/net/xtmrt/impl$(CONFIG_BCM_XTMRT_IMPL))
JMK_SUBDIRS+=$(if $(CONFIG_BCM_ENET),opensource/net/enet)
JMK_SUBDIRS+=$(if $(CONFIG_BCM_USB),broadcom/net/usb/impl3)
JMK_SUBDIRS+=$(if $(CONFIG_BCM_ADSL),broadcom/char/adsl/impl1)
JMK_SUBDIRS+=$(if $(CONFIG_BCM_XTMCFG),broadcom/char/xtmcfg/impl$(CONFIG_BCM_XTMCFG_IMPL))
JMK_SUBDIRS+=$(if $(CONFIG_BCM_ENDPOINT),broadcom/char/endpoint/impl1)
JMK_SUBDIRS+=$(if $(CONFIG_BCM_BCMDSP),broadcom/char/dspapp/impl1)
JMK_SUBDIRS+=$(if $(CONFIG_BCM_WLAN),broadcom/net/wl/impl16)
JMK_SUBDIRS+=$(if $(CONFIG_BCM_FAP),broadcom/char/fap/impl1)
JMK_SUBDIRS+=$(if $(CONFIG_BCM_PWRMNGT),broadcom/char/pwrmngt/impl1)
JMK_SUBDIRS+=$(if $(CONFIG_BCM_BPM),broadcom/char/bpm/impl1)
JMK_SUBDIRS+=$(if $(CONFIG_BCM_INGQOS),broadcom/char/ingqos/impl1)
JMK_SUBDIRS+=$(if $(CONFIG_BCM_SPU),broadcom/char/spudd/impl2)
JMK_SUBDIRS+=$(if $(CONFIG_SER_AUQ00X_MCU_BCM_LEDS),broadcom/char/sercomm_mcu/impl1)

JMK_JPKG_EXPORTED_DIR+=broadcom/include/bcm963xx
JMK_JPKG_EXPORTED_FILES+=opensource/net/enet/shared/bcmenet.h

include $(JMKE_MK)

