ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../..
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JMK_ARCHCONFIG_FIRST_TASKS+=$(JMKE_PWD_BUILD)/jquery.js

JMK_SUBDIRS+=themes demos ui

jquery_build_js=$(JMKE_PWD_BUILD)/jquery.js
jquery_src_js=jquery.scrollable.js jqSOAPClient.js jquery.flot.js excanvas.js \
  jquery.form.js jquery.combobox.js jquery.flot.navigate.js jquery.ba-bbq.js \
  jquery.jungo.js jquery.jscrollpane.js jquery.mousewheel.js jquery.md5.js \
  jquery.blockUI.js jquery.browser.js jquery.gritter.js jquery.easing.js \
  jquery.quicksearch.js jquery.truncate.js ICanHaz.min.js ua-parser.js
JMK_JS_FILES=$(jquery_build_js) $(jquery_src_js)

$(call JMKF_RAMDISK_FILES,$(addprefix $(JMKE_PWD_SRC)/,$(jquery_src_js)), \
  $(ramdisk_js_path))
$(call JMKF_RAMDISK_FILES,$(jquery_build_js),$(ramdisk_js_path))

ifdef CONFIG_VF_WBM_INTERNATIONAL
jquery_ver=1.11.1
else
ifdef CONFIG_RG_VODAFONE_IT
jquery_ver=1.6.1
endif
ifdef CONFIG_RG_VODAFONE_UK
jquery_ver=1.6.1
endif
ifdef CONFIG_RG_VODAFONE_NZ
jquery_ver=1.6.1
endif
ifdef CONFIG_RG_VODAFONE_ES
jquery_ver=1.6.4
endif
ifdef CONFIG_RG_VODAFONE_PT
jquery_ver=1.6.4
endif
endif # CONFIG_VF_WBM_INTERNATIONAL

ifdef CONFIG_RG_JSMIN
$(JMKE_PWD_BUILD)/jquery.js: $(JMKE_PWD_SRC)/jquery-$(jquery_ver).min.js
	$(JMKE_LN) $< $@
else
$(JMKE_PWD_BUILD)/jquery.js: $(JMKE_PWD_SRC)/jquery-$(jquery_ver).js
	$(JMKE_LN) $< $@
endif

$(call JMKE_INCLUDE_RULES)
