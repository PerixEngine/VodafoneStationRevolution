ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../..
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JMK_SUBDIRS+=modules
JMK_SUBDIRS+=$(if $(CONFIG_RG_HUAWEI_CDC_ETHER_NCM), rg)

$(call JMKE_INCLUDE_RULES)
