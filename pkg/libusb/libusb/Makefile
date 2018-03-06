ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JMK_CFLAGS+=-I$(JMKE_PWD_BUILD)/../
JMK_EXPORT_HEADERS+=libusb.h
JMK_INTERNAL_HEADERS+=version.h libusbi.h libusb.h

JMK_SUBDIRS+=os

ifdef CONFIG_DYN_LINK
  LIB_USB10=libusb-1.0.so
  JMK_SO_TARGET=libusb-1.0.so
  JMK_RAMDISK_LIB_FILES+=libusb-1.0.so
else
  LIB_USB10=libusb-1.0.a
  JMK_A_TARGET=libusb-1.0.a
endif

JMK_O_OBJS_$(LIB_USB10)+=sync.o io.o core.o descriptor.o os/linux.o
JMK_EXPORT_LIBS+=$(LIB_USB10)

$(call JMKE_INCLUDE_RULES)
