ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../..
  include $(JMK_ROOT)/jmk/env_root.mak
endif

ifneq ($(CONFIG_RG_VOIP_REDUCE_SUPPORT)$(CONFIG_RG_FULL_PBX)$(CONFIG_RG_VOIP_VOICEMAIL),)
  JMK_RAMDISK_FILES=$(call Asterisk_Ramdisk, beep.gsm, /var/lib/asterisk/sounds)
endif

ifdef CONFIG_RG_FULL_PBX
  JMK_RAMDISK_FILES+=$(foreach f, $(notdir \
    $(wildcard $(JMKE_PWD_SRC)/queue-*.gsm) invalid.gsm \
    pbx-invalid.gsm), $(call Asterisk_Ramdisk, $f, /var/lib/asterisk/sounds))
endif

ifndef CONFIG_RG_VOIP_ASTERISK_VENDOR_SOUNDS
ifdef CONFIG_RG_VOIP_VOICEMAIL
  JMK_RAMDISK_FILES+=$(foreach f, $(notdir $(wildcard $(JMKE_PWD_SRC)/vm-*.gsm) \
    auth-thankyou.gsm greet0010.gsm \
    pbx-invalid.gsm), $(call Asterisk_Ramdisk, $f, /var/lib/asterisk/sounds))
endif

ifneq ($(CONFIG_RG_FULL_PBX)$(CONFIG_RG_VOIP_VOICEMAIL),)
  JMK_SUBDIRS+=digits
endif
endif

$(call JMKE_INCLUDE_RULES)
