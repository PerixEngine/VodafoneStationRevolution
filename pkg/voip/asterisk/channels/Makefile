ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../..
  include $(JMK_ROOT)/jmk/env_root.mak
endif
include $(JMK_ROOT)/pkg/voip/asterisk/common.mak

JMK_EXPORT_HEADERS_DIR=voip/asterisk/channels

JMK_A_TARGET=libchannels.a

ifdef CONFIG_ZAPTEL
  JMK_O_OBJS+=chan_zap.o
  AST_MODULES_WITH_RELOAD+=chan_zap
else
ifdef CONFIG_IXJ
  JMK_O_OBJS+=chan_phone.o
  AST_MODULES_NO_RELOAD+=chan_phone
else
ifdef CONFIG_HW_DSP
  JMK_O_OBJS+=chan_jdsp.o
  AST_MODULES_WITH_RELOAD_IF_CHANGED+=chan_jdsp
  JMK_CFLAGS+=-DDSP_ENABLED
endif
endif
endif

ifdef CONFIG_RG_3G_VOICE
  JMK_O_OBJS+=chan_sebi.o
  AST_MODULES_WITH_RELOAD_IF_CHANGED+=chan_sebi
  JMK_EXPORT_HEADERS+=chan_sebi.h
endif

ifdef CONFIG_RG_DECT
  JMK_O_OBJS+=chan_dect.o dect_cc.o
  AST_MODULES_WITH_RELOAD_IF_CHANGED+=chan_dect
  JMK_CFLAGS+=-DDECT_ENABLED
endif

ifdef CONFIG_HW_FXO
  JMK_O_OBJS+=chan_jfxo.o
  AST_MODULES_WITH_RELOAD+=chan_jfxo
endif

ifdef CONFIG_RG_VOIP_ASTERISK_H323
  JMK_SUBDIRS+=h323
  JMK_O_OBJS+=chan_h323.o h323/target.o
  AST_MODULES_WITH_RELOAD+=chan_h323
endif

ifdef CONFIG_RG_VOIP_ASTERISK_SIP
  JMK_O_OBJS+=chan_sip.o
  AST_MODULES_WITH_RELOAD_IF_CHANGED+=chan_sip
endif

ifdef CONFIG_RG_VOIP_ASTERISK_CAPI
  JMK_CFLAGS+=-I./chan_capi
  JMK_SUBDIRS+=chan_capi
  JMK_O_OBJS+=chan_capi/channel_capi.o
  AST_MODULES_WITH_RELOAD+=channel_capi
endif

ifdef CONFIG_RG_VOIP_ASTERISK_MGCP_GW
  JMK_O_OBJS+=chan_mgcp_gw.o
  AST_MODULES_WITH_RELOAD+=chan_mgcp_gw
endif

ifdef CONFIG_RG_VOIP_ASTERISK_MGCP_CALL_AGENT
  JMK_O_OBJS+=chan_mgcp.o
  AST_MODULES_WITH_RELOAD+=chan_mgcp
endif

include $(JMK_ROOT)/pkg/voip/asterisk/modules.mak

$(foreach t, $(JMK_SO_TARGET), $(eval JMK_O_OBJS_$t:=$(t:%.so=%.o)))

JMK_CFLAGS+=-DT38_SUPPORT

JMK_RAMDISK_FILES+=$(foreach f, $(notdir $(JMK_SO_TARGET)), \
  $(call Lib_Asterisk_Ramdisk, $f))

$(call JMKE_INCLUDE_RULES)
