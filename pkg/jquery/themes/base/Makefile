ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../..
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JMK_SUBDIRS=images

JMK_CSS_FILES+=ui.all.css ui.core.css ui.theme.css ui.allplugins.css \
  ui.accordion.css ui.progressbar.css ui.slider.css ui.datepicker.css \
  ui.tabs.css ui.dialog.css ui.resizable.css 

ifdef CONFIG_RG_JNET_SERVER
  ramdisk_css_path=$(JRMS_UI_CSS_DST)
endif
ifdef CONFIG_RG_VAS_PORTAL
  ramdisk_css_path=/vas/jquery/themes/base/
endif

ifdef ramdisk_css_path
  $(call JMKF_RAMDISK_FILES,$(addprefix $(JMKE_PWD_SRC)/,$(JMK_CSS_FILES)), \
    $(ramdisk_css_path))
endif

$(call JMKE_INCLUDE_RULES)
