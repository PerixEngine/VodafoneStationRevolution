ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../../..
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JQUERY_BASE_IMGS=222222_256x240_icons_icons.png 2e83ff_256x240_icons_icons.png \
       454545_256x240_icons_icons.png 888888_256x240_icons_icons.png \
       cccccc_40x100_textures_03_highlight_soft_75.png \
       cd0a0a_256x240_icons_icons.png dadada_40x100_textures_02_glass_75.png \
       e6e6e6_40x100_textures_02_glass_75.png \
       fbf9ee_40x100_textures_02_glass_55.png \
       fef1ec_40x100_textures_05_inset_soft_95.png \
       ffffff_40x100_textures_02_glass_65.png \
       ffffff_40x100_textures_02_glass_75.png

JMK_CD_EXPORTED_FILES+=$(JQUERY_BASE_IMGS)

ifdef CONFIG_RG_VAS_PORTAL
  ramdisk_img_dir=/vas/jquery/themes/base/images
endif
ifdef CONFIG_RG_JNET_SERVER
  ramdisk_img_dir=$(JRMS_UI_IMG_DST)
endif
ifneq ($(CONFIG_RG_VAS_PORTAL)$(CONFIG_RG_JNET_SERVER),)
  $(call JMKF_RAMDISK_FILES,$(addprefix $(JMKE_PWD_SRC)/,$(JQUERY_BASE_IMGS)), \
    $(ramdisk_img_dir))
endif

$(call JMKE_INCLUDE_RULES)
