ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../../..
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JMK_RAMDISK_FILES=$(foreach f, $(notdir $(wildcard $(JMKE_PWD_SRC)/*.gsm)), \
  $(call Asterisk_Ramdisk, $f, /var/lib/asterisk/sounds/digits))

$(call JMKE_INCLUDE_RULES)

