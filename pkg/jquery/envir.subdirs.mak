INSTALL=install -v

ifdef CONFIG_OPENRG
  ramdisk_js_path=/home/httpd/html/jquery
endif

ifdef CONFIG_RG_JNET_SERVER
  ramdisk_js_path=$(JRMS_UI_JS_DST)
endif

ifdef CONFIG_RG_VAS_PORTAL
  ramdisk_js_path=/vas/jquery
endif

$(if $(ramdisk_js_path),,$(error ramdisk_js_path must be defined))

VAS_TGZ_DIR=$(JMK_ROOT)/pkg/vas/install

ifndef CONFIG_RG_FILES_ON_HOST
  RAMDISK_2_DLM=vodafonewbm
endif
