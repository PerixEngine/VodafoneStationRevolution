ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../..
  include $(JMK_ROOT)/jmk/env_root.mak
endif

ifneq ($(CONFIG_RG_VODAFONE_IT)$(CONFIG_RG_VODAFONE_UK)$(CONFIG_RG_VODAFONE_DE)$(CONFIG_RG_VODAFONE_NZ),)
JMK_JS_FILES+=ui.core.js ui.sortable.js effects.core.js effects.blind.js \
  effects.scale.js ui.dialog.js ui.draggable.js ui.selectable.js \
  ui.droppable.js effects.shake.js ui.datepicker.js ui.progressbar.js
endif
ifdef CONFIG_RG_VODAFONE_ES
JMK_JS_FILES+=jquery.ui.all.js
endif

ifdef CONFIG_RG_VAS_PORTAL
  ramdisk_js_path:=$(ramdisk_js_path)/ui
endif

$(call JMKF_RAMDISK_FILES,$(addprefix $(JMKE_PWD_SRC)/,$(JMK_JS_FILES)), \
  $(ramdisk_js_path))

$(call JMKE_INCLUDE_RULES)
