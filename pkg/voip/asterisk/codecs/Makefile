ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../..
  include $(JMK_ROOT)/jmk/env_root.mak
endif
include $(JMK_ROOT)/pkg/voip/asterisk/common.mak

JMK_A_TARGET=libcodecs.a
JMK_O_OBJS=codec_ulaw.o codec_alaw.o codec_a_mu.o codec_g726.o
AST_MODULES_WITH_RELOAD+=$(if $(CONFIG_RG_VOIP_ASTERISK_CODEC_ULAW),codec_ulaw) \
  $(if $(CONFIG_RG_VOIP_ASTERISK_CODEC_ALAW),codec_alaw) \
  $(if $(CONFIG_RG_VOIP_ASTERISK_CODEC_G726),codec_g726)
AST_MODULES_NO_RELOAD+=$(if $(CONFIG_RG_VOIP_ASTERISK_CODEC_AMULAW),codec_a_mu)

include $(JMK_ROOT)/pkg/voip/asterisk/modules.mak

SO_RAMDISK_SUBDIR=asterisk/

$(call JMKE_INCLUDE_RULES)
