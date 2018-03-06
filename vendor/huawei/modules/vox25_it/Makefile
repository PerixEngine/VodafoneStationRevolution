ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../..
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JMK_SUBDIRS+=$(if $(CONFIG_TSL2672), tsl2672)
JMK_SUBDIRS+=$(if $(CONFIG_RG_KLEDS), tca6507)

$(call JMKE_INCLUDE_RULES)
