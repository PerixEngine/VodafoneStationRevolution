ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../..
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JMK_CSS_FILES+=demos.css

ifdef CONFIG_RG_VAS_PORTAL
  $(call JMKF_RAMDISK_FILES,$(addprefix $(JMKE_PWD_SRC)/,$(JMK_CSS_FILES)), \
    $(ramdisk_js_path)/demos)
endif

$(call JMKE_INCLUDE_RULES)
