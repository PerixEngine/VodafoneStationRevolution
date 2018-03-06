JMK_ROOT=../../../..
include $(JMK_ROOT)/envir.mak

ifdef CONFIG_RG_KRGLDR
JMK_MOD_TARGET=krgldr_module.o
JMK_RAMDISK_MODULES_FILES+=$(JMK_MOD_TARGET)

ifeq ($(CONFIG_RG_KRGLDR),y)
  JMK_MOD_2_STAT+=$(JMK_MOD_TARGET)
endif

JMK_O_OBJS_krgldr_module.o=krgldr.o

endif

JMK_EXPORT_HEADERS_DIR=kernel/linux/krgldr
JMK_EXPORT_HEADERS+=krgldr.h

JMK_RAMDISK_FIRST_TASKS+=rg_krgldr_dev

rg_krgldr_dev:
	$(JMKE_MKNOD) $(JMKE_RAMDISK_DEV_DIR)/rg_krgldr c 10 242

include $(JMKE_MK)
