ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../..
  include $(JMK_ROOT)/jmk/env_root.mak
endif
include $(JMK_ROOT)/pkg/voip/asterisk/common.mak

JMK_A_TARGET=libfuncs.a
JMK_O_OBJS=pbx_functions.o
AST_MODULES_NO_RELOAD+=pbx_functions

include $(JMK_ROOT)/pkg/voip/asterisk/modules.mak

JMK_CFLAGS+=-DBUILTIN_FUNC

JMK_RAMDISK_FILES+=$(foreach f, $(notdir $(JMK_SO_TARGET)), \
  $(call Lib_Asterisk_Ramdisk, $f))

$(foreach t, $(JMK_SO_TARGET), $(eval JMK_O_OBJS_$t:=$(t:%.so=%.o)))

$(call JMKE_INCLUDE_RULES)
