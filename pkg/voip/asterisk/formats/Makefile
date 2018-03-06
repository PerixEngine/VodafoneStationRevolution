ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../..
  include $(JMK_ROOT)/jmk/env_root.mak
endif
include $(JMK_ROOT)/pkg/voip/asterisk/common.mak

JMK_A_TARGET=libformats.a
JMK_O_OBJS=format_au.o format_g729.o format_g726.o
AST_MODULES_NO_RELOAD+=format_au format_g729 format_g726

include $(JMK_ROOT)/pkg/voip/asterisk/modules.mak

SO_RAMDISK_SUBDIR=asterisk/

$(call JMKE_INCLUDE_RULES)
