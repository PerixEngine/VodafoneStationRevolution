JMK_ROOT=../../../../../../..
include $(JMK_ROOT)/envir.mak

JMK_EXPORT_HEADERS_DIR=vendor/broadcom/bcm9636x
JMK_EXPORT_HEADERS+=flash_common.h boardparms.h
ifdef CONFIG_BCM96362
JMK_EXPORT_HEADERS+=6362_map_part.h bcm_hwdefs.h 6362_intr.h
endif
ifdef CONFIG_BCM963268
JMK_EXPORT_HEADERS+=63268_map_part.h bcm_hwdefs.h 63268_intr.h flash_common.h
endif

JMK_AUTOGEN_SRC+=dsp_mod_size.h

include $(JMKE_MK)
