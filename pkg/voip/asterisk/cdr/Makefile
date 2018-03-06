ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../..
  include $(JMK_ROOT)/jmk/env_root.mak
endif
include $(JMK_ROOT)/pkg/voip/asterisk/common.mak

JMK_A_TARGET=libcdr.a
JMK_O_OBJS=cdr_manager.o
AST_MODULES_NO_RELOAD+=cdr_manager

#The problem with sparc is the best stuff is in newer versions of gcc (post 3.0) only.
#This works for even old (2.96) versions of gcc and provides a small boost either way.
#A ultrasparc cpu is really v9 but the stock debian stable 3.0 gcc doesn.t support it.
#So we go lowest common available by gcc and go a step down, still a step up from
#the default as we now have a better instruction set to work with. - Belgarath
ifeq ($(PROC),sparc64)
 PROC=ultrasparc
  JMK_CFLAGS += -mtune=$(PROC) -O3 -pipe -fomit-frame-pointer -mcpu=v8
endif


include $(JMK_ROOT)/pkg/voip/asterisk/modules.mak

JMK_RAMDISK_FILES+=$(foreach f, $(notdir $(JMK_SO_TARGET)), \
  $(call Lib_Asterisk_Ramdisk, $f))

$(foreach t, $(JMK_SO_TARGET), $(eval JMK_O_OBJS_$t:=$(t:%.so=%.o)))
$(call JMKE_INCLUDE_RULES)
