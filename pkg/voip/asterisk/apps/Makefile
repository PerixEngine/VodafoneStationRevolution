ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../..
  include $(JMK_ROOT)/jmk/env_root.mak
endif
include $(JMK_ROOT)/pkg/voip/asterisk/common.mak

JMK_A_TARGET=libapps.a
JMK_O_OBJS=app_dial.o app_openrg_cmd.o app_transfer.o app_while.o \
	   app_softhangup.o 
AST_MODULES_NO_RELOAD+=app_dial app_openrg_cmd app_transfer app_while \
		       app_softhangup 

ifdef CONFIG_RG_VOIP_VOICEMAIL
  ifdef CONFIG_RG_SQLITE
    JMK_CFLAGS+=-DUSE_SQLITE_DB
  endif
  JMK_O_OBJS+=app_voicemail.o app_sayunixtime.o
  AST_MODULES_WITH_RELOAD+=app_voicemail
  AST_MODULES_NO_RELOAD+=app_sayunixtime
endif

ifdef CONFIG_RG_VOIP_HUNT_GROUP
  JMK_O_OBJS+=app_queue.o
  AST_MODULES_WITH_RELOAD+=app_queue
endif

ifdef CONFIG_RG_PBX
  JMK_O_OBJS+=app_macro.o app_echo.o app_stack.o
  AST_MODULES_NO_RELOAD+=app_macro app_echo app_stack
  ifneq ($(CONFIG_RG_FULL_PBX)$(CONFIG_RG_VOIP_REDUCE_SUPPORT)$(CONFIG_RG_CALLWAITPROV),)
    JMK_O_OBJS+=app_playback.o
    AST_MODULES_NO_RELOAD+=app_playback
  endif
  ifdef CONFIG_RG_VODAFONE_ACTIVATION_WIZARD
    JMK_O_OBJS+=app_cwmp.o
    AST_MODULES_NO_RELOAD+=app_cwmp
  endif
  ifdef CONFIG_RG_VODAFONE
    JMK_O_OBJS+=app_read.o app_readfile.o app_record.o app_system.o 
    AST_MODULES_NO_RELOAD+=app_read app_readfile app_record app_system
  endif
  ifdef CONFIG_RG_FULL_PBX
    JMK_O_OBJS+=app_record.o app_parkandannounce.o
    AST_MODULES_NO_RELOAD+=app_record app_parkandannounce
  endif
endif

ifdef CONFIG_RG_PHONEBOOK_DB
  JMK_O_OBJS+=app_phonebook_db.o
  AST_MODULES_NO_RELOAD+=app_phonebook_db
endif

include $(JMK_ROOT)/pkg/voip/asterisk/modules.mak

JMK_RAMDISK_FILES+=$(foreach f, $(notdir $(JMK_SO_TARGET)), \
  $(call Lib_Asterisk_Ramdisk, $f))

$(foreach t, $(JMK_SO_TARGET), $(eval JMK_O_OBJS_$t:=$(t:%.so=%.o)))

$(call JMKE_INCLUDE_RULES)
