ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../..
  include $(JMK_ROOT)/jmk/env_root.mak
endif
include $(JMK_ROOT)/pkg/voip/asterisk/common.mak

JMK_A_TARGET=libpbx.a
JMK_O_OBJS=pbx_config.o
AST_MODULES_WITH_RELOAD_IF_CHANGED+=pbx_config

include $(JMK_ROOT)/pkg/voip/asterisk/modules.mak

JMK_RAMDISK_FILES+=$(foreach f, $(notdir $(JMK_SO_TARGET)), \
  $(call Lib_Asterisk_Ramdisk, $f))

$(call JMKE_INCLUDE_RULES)
