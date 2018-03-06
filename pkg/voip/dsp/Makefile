JMK_ROOT=../../..
include $(JMK_ROOT)/envir.mak

JMK_EXPORT_HEADERS_DIR=voip/dsp
JMK_EXPORT_HEADERS+=phone.h phone_modules.h fxo_module.h

JMK_EXPORT_AS_SRC+=phone_conf.c

ifndef CONFIG_RG_GPL
JMK_MOD_TARGET=phone_mod.o
JMK_RAMDISK_MODULES_FILES+=$(JMK_MOD_TARGET)
JMK_OX_OBJS_phone_mod.o=phone.o
JMK_O_OBJS_phone_mod.o=phone_conf.o
JMK_MOD_CFLAGS+=-I../
endif

include $(JMKE_MK)
