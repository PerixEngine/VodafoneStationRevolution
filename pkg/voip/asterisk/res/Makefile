ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../..
  include $(JMK_ROOT)/jmk/env_root.mak
endif
include $(JMK_ROOT)/pkg/voip/asterisk/common.mak

JMK_A_TARGET=libres.a
JMK_O_OBJS=res_features.o res_adsi.o res_monitor.o
AST_MODULES_NO_RELOAD=res_monitor
AST_MODULES_WITH_RELOAD=res_features res_adsi

ifdef CONFIG_RG_VOIP_MOH
  JMK_O_OBJS+=res_musiconhold.o
  AST_MODULES_WITH_RELOAD+=res_musiconhold
endif

include $(JMK_ROOT)/pkg/voip/asterisk/modules.mak

JMK_RAMDISK_FILES+=$(foreach f, $(notdir $(JMK_SO_TARGET)), \
  $(call Lib_Asterisk_Ramdisk, $f))

$(foreach t, $(JMK_SO_TARGET), $(eval JMK_O_OBJS_$t:=$(t:%.so=%.o)))

$(call JMKE_INCLUDE_RULES)
