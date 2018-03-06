ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JMK_TARGET=comgt
JMK_RAMDISK_BIN_FILES+=$(JMK_TARGET)
JMK_O_OBJS=comgt.o

JMK_INTERNAL_HEADERS+=comgt.h

JMK_RAMDISK_FILES+=$(JMKE_PWD_SRC)/connect_script__/etc/comgt/connect_script
JMK_JPKG_EXPORTED_FILES+=connect_script

$(JMKE_INCLUDE_RULES)
