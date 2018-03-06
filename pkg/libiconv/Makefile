ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JMK_SUBDIRS+=lib libcharset/lib

JMK_EXPORT_HEADERS_DIR+=libiconv
JMK_EXPORT_HEADERS+=include/iconv.h
JMK_INTERNAL_HEADERS+=config.h

ifdef CONFIG_DYN_LINK
  LIB_ICONV=libiconv.so
  JMK_SO_TARGET=libiconv.so
  JMK_RAMDISK_LIB_FILES+=$(JMK_SO_TARGET)
else
  LIB_ICONV=libiconv.a
  JMK_A_TARGET=libiconv.a
endif

JMK_O_OBJS_$(LIB_ICONV)+=lib/libiconv.o libcharset/lib/lcharset.o

LOCAL_LIB_ICONV=local_libiconv.o
JMK_LOCAL_O_TARGET+=$(LOCAL_LIB_ICONV)
JMK_O_OBJS_$(LOCAL_LIB_ICONV)+=lib/libiconv.o libcharset/lib/lcharset.o

JMK_EXPORT_LIBS+=$(LIB_ICONV)

$(call JMKE_INCLUDE_RULES)
