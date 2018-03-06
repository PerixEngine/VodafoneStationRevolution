JMK_ROOT= ../
include $(JMK_ROOT)/envir.mak

ifndef CONFIG_RG_OS
ifdef JMKE_IS_BUILDDIR
  $(error CONFIG_RG_OS not defined!)
endif
endif

ifdef CONFIG_RG_OS_LINUX
  JMK_ARCHCONFIG_FIRST_TASKS+=cp_kernel
endif

ifdef CONFIG_RG_OS_LINUX_26
  LINUX_DIR=linux-2.6
endif
ifdef CONFIG_RG_OS_LINUX_3X
  LINUX_DIR=linux-3.x
endif

LINUX_AUTOCONF_H=$(JMKE_BUILDDIR)/os/$(LINUX_DIR)/$(AUTOCONF_H)
LINUX_AUTO_DOT_CONF=$(JMKE_BUILDDIR)/os/$(LINUX_DIR)/$(AUTO_DOT_CONF)

JMK_ARCHCONFIG_FIRST_TASKS+=os_create_links
JMK_ARCHCONFIG_FIRST_TASKS+=$(if $(CONFIG_RG_OS_LINUX), prepare_linux)

KERNEL_DIR=$(CURDIR)/kernel

JMK_CLEAN+=$(wildcard $(STATIC_MOD_DIR)/*) $(JMKE_BUILDDIR)/$(notdir $(OPENRG_IMG)) $(JMKE_BUILDDIR)/$(notdir $(OPENRG_PROD)) layout.bin layout.sec

ifndef CONFIG_RG_JPKG
  JMK_OTHER_TASKS+=$(OPENRG_IMG) $(JMKE_BUILDDIR)/$(notdir $(OPENRG_IMG)) make_kernel
endif

ifndef CONFIG_RG_GPL
  ifndef CONFIG_RG_JPKG
    ifeq ($(CONFIG_RG_PROD_IMG),y)
      JMK_OTHER_TASKS+=$(OPENRG_PROD) $(JMKE_BUILDDIR)/$(notdir $(OPENRG_PROD))
    else  
      ifneq ($(CONFIG_RG_NETWORK_BOOT_IMAGE),y)
        JMK_OTHER_TASKS+=$(if $(CONFIG_OPENRG)$(CONFIG_RG_RGLOADER),image_size_verify)
      endif
    endif
    ifeq ($(CONFIG_RG_NETWORK_BOOT_IMAGE),y)
network_boot_message: $(JMK_OTHER_TASKS)

      JMK_OTHER_TASKS+=network_boot_message
    else
end_message: $(JMK_OTHER_TASKS)

      JMK_OTHER_TASKS+=end_message
    endif
  endif
else
  $(shell touch $(JMKE_BUILDDIR)/os/linux/arch/$(ARCH)/ramdisk/mod.img)
  $(shell touch $(JMKE_BUILDDIR)/os/linux/arch/$(ARCH)/ramdisk/ramdisk.gz)
  $(shell touch $(JMKE_BUILDDIR)/os/linux/arch/$(ARCH)/cramdisk/cramfs.img)
  $(shell touch $(JMKE_BUILDDIR)/os/linux/arch/$(ARCH)/cramdisk/cramfs_init.img)
  $(shell touch $(JMKE_BUILDDIR)/os/linux/needed_symbols.lst)
endif
  
ifdef CONFIG_RG_OS_LINUX
  JMK_CD_EXPORTED_FILES+=Rules_common.make Makefile.vmlinuz
endif
ifdef CONFIG_RG_OS_LINUX_26
  JMK_JPKG_EXPORTED_DIR_SRC+=linux-2.6
endif
ifdef CONFIG_RG_OS_LINUX_3X
  JMK_JPKG_EXPORTED_DIR_SRC+=linux-3.x
endif

ifdef CONFIG_RG_FACTORY_SETTINGS
  FACTORY_SETTINGS=$(JMKE_BUILDDIR)/pkg/main/rg_factory
endif

ifndef CONFIG_RG_JPKG
  ifneq ($(MAKECMDGOALS),archconfig)
# Adding explicit all: I have to include the boot Makefile before rg.mak for
# distribution purposes but I want the all: the be the first rule. (Ugly? yes,
# working? don't touch)
all:

    include Makefile.vmlinuz
  endif
endif

ifeq ($(CONFIG_RG_RGLOADER),y)
    SECTION_TYPE="\<BOOT\>"
else
    SECTION_TYPE="\<IMAGE\>"
endif

JMK_RAMDISK_FIRST_TASKS+=kernel_ramdisk

include $(JMKE_MK)
include $(JMK_ROOT)/create_includes.mak
ifeq ($(CONFIG_BOOTLDR_SIGNED_IMAGES),y)
  include $(JMK_ROOT)/rmt.mak
endif

$(OPENRG_IMG): dummy

end_message: dummy
	@echo
	@echo "OpenRG rg_factory is ready at $(FACTORY_SETTINGS)"
	@echo "OpenRG image is ready at $(OPENRG_IMG)"
ifneq ($(CONFIG_RG_UML),)
	@echo "To run this image: "
	@echo "$$ jrguml burn    # 'burn' (load) the new image"
	@echo "$$ jnetwork start # connect uml0 (WAN) and uml1 (LAN) interfaces to UML"
	@echo "$$ jrguml start --std # start running OpenRG/UML"
	@echo "To later stop OpenRG/UML, run 'jrguml stop' from a DIFFERENT terminal"
	@echo "For further assistance see pkg/tools/README"
endif

network_boot_message: dummy
ifeq ($(findstring $(DIST), BCM96358), $(DIST))
	@echo
	@echo "Copy the following file to /tftpboot in order to allow network-booting:"
	@echo $$ cp $(JMKE_BUILDDIR)/vendor/broadcom/bcm963xx/linux-2.6/boot/bootimg.elf /tftpboot
	@echo
endif

$(JMKE_BUILDDIR)/$(notdir $(OPENRG_IMG)): $(notdir $(OPENRG_IMG))
	@$(JMKE_LN) $(OPENRG_IMG) $@

ifeq ($(CONFIG_RG_PROD_IMG),y)
  ifeq ($(findstring $(DIST), RGLOADER_RTA770W RGLOADER_WADB100G), $(DIST))
    # Create RGLoader image: 'jump boot' and 'boot' sections only
    BOOTSTRAP_CMD_PARAM=-s '0 ../vendor/broadcom/bcm963xx/boot/jmp_boot.img'
    BOOTSTRAP=$(OPENRG_IMG)
    # No factory settings or OpenRG image when creating RGLoader
    FACTORY_SETTINGS=
    PROD_SKIP_IMAGE=y
  endif
  ifdef BOOTSTRAP
    BOOTSTRAP_CMD_PARAM+=-b $(BOOTSTRAP)
  endif
  ifdef FACTORY_SETTINGS
    FACTORY_SETTINGS_CMD_PARAM=-f $(FACTORY_SETTINGS)
  endif
  ifneq ($(PROD_SKIP_IMAGE),y)
    IMAGE_PARAM=-i $(if $(CONFIG_BOOTLDR_SIGNED_IMAGES),\
		$(OPENRG_RMT_UPDATE_RSA_IMG),$(OPENRG_IMG))
  endif
$(OPENRG_PROD): $(OPENRG_IMG) $(if $(CONFIG_BOOTLDR_SIGNED_IMAGES),$(OPENRG_RMT_UPDATE_RSA_IMG)) $(BOOTSTRAP) $(FACTORY_SETTINGS)
	$(JMKE_BUILDDIR)/pkg/perm_storage/build_flash -v $(BOOTSTRAP_CMD_PARAM) \
	  $(FACTORY_SETTINGS_CMD_PARAM) $(FLASH_LAYOUT_CMD_PARAM) \
	  $(IMAGE_PARAM) -p $(FLASH_IMG_OFFSET)
	cp flash.img $@
ifneq ($(CONFIG_RG_PERM_STORAGE_UBI)$(CONFIG_RG_UBIFS_UBI_VOLUMES_STR),)
	$(JMK_ROOT)/pkg/perm_storage/build_ubi_image.sh $(JMKE_BUILDDIR)/pkg/mtd-utils/ubi-utils/src/ubinize flash.img ubi.img
	$(JMKE_BUILDDIR)/pkg/mtd-utils/nandoob -o ubi.oob.img -p $(CONFIG_HW_NAND_MIN_IO_SIZE) \
	  ubi.img
endif

$(JMKE_BUILDDIR)/$(notdir $(OPENRG_PROD)): $(notdir $(OPENRG_PROD))
	@$(JMKE_LN) $(OPENRG_PROD) $@
else	
image_size_verify: $(OPENRG_IMG)
ifdef JMKE_IS_BUILDDIR
	$(JMKE_BUILDDIR)/pkg/perm_storage/build_flash
ifndef CONFIG_RG_IGNORE_IMAGE_SECTION_SIZE
ifndef CONFIG_RG_RGLOADER
	@echo -e "\n\n\n\n\n\n\n    ===============================================================\n"
	@echo -e "    use $(JMKE_BUILDDIR)/pkg/kernel/linux/boot/image.elf for network boot\n"
	@echo -e "    ===============================================================\n\n\n\n\n\n\n"
endif
	@section_size=`$(JMKE_BUILDDIR)/pkg/perm_storage/build_flash  | grep $(SECTION_TYPE) | head -n 1 | awk 'BEGIN { RS="\r\n"} { print $$8}'`; \
	image_size=`du -b -D $(OPENRG_IMG) | awk '{print $$1}'`; \
	free_space=$$((section_size - image_size)); \
	echo "section size = $$((section_size)) , image size = $$image_size" \
	    ", free space = $$free_space"; \
	if (((section_size>0)) && ((image_size>section_size))); then \
	    echo "Image too big (exceeds section size by "\
	        $$((image_size - section_size)) " bytes)."; \
	    exit 1; \
	fi;
endif
endif
endif

modules:
ifdef JMKE_IS_BUILDDIR
	$(MAKE) -C $(KERNEL_DIR) $@
endif

cp_kernel:
	$(MKDIR) $(JMKE_BUILDDIR)/os/
ifdef CONFIG_RG_OS_LINUX
	$(MKDIR) $(JMKE_BUILDDIR)/os/$(LINUX_DIR)
endif
	ln -sfn $(JMK_ROOT)/os/Makefile.vmlinuz $(JMKE_BUILDDIR)/os
	ln -sfn $(JMK_ROOT)/os/Makefile $(JMKE_BUILDDIR)/os
	ln -sfn $(JMK_ROOT)/os/Rules_common.make $(JMKE_BUILDDIR)/os

os_create_links:
ifdef CONFIG_RG_OS_LINUX
	$(JMKE_LN) $(CURDIR)/$(LINUX_DIR) $(CURDIR)/linux
	@$(JMKE_LN) $(CURDIR)/linux $(KERNEL_DIR)
	@$(MKDIR) $(STATIC_MOD_DIR)
else
endif
LINUX_DOT_CONFIG_FILE=$(JMKE_BUILDDIR)/os/$(LINUX_DIR)/.config
$(LINUX_AUTOCONF_H):
	$(MKDIR) $(@D)
	@cat $(RG_CONFIG_H) | grep -v "extern " | grep -vw _RG_CONFIG_H_ | \
	  grep -v "^#endif" > $@

$(LINUX_AUTO_DOT_CONF):
	$(MKDIR) $(@D)
	touch $@.cmd
	# kernel verifies config files timestamps - .config should be created
	# before auto.conf for kernel to start the compilation
	@echo "# Jungo auto-generated .config file" > $(LINUX_DOT_CONFIG_FILE)
	@echo "# This file is dummy - kernel configs are set in " $(LINUX_AUTO_DOT_CONF) \
	  >> $(LINUX_DOT_CONFIG_FILE)
	@echo "# Jungo auto-generated auto.conf file" > $@
	@echo "include $(CONFIG_FILE)" >> $@
#        Replace the CC compiler from rg_gcc wrapper with the "real"
#        (unwrapped) TARGET_CC.
	@echo "CC=$(TARGET_CC)" >> $@
	@echo "CPP=$(TARGET_CPP)" >> $@
ifdef CONFIG_RG_OS_LINUX_3X
	touch $(JMKE_BUILDDIR)/os/$(LINUX_DIR)/include/config/tristate.conf
endif

prepare_linux: $(LINUX_AUTOCONF_H) $(LINUX_AUTO_DOT_CONF)
	$(MAKE) -C $(JMK_ROOT)/os/$(LINUX_DIR)/ O=$(JMKE_BUILDDIR)/os/$(LINUX_DIR)/ scripts
	$(MAKE) -C $(JMK_ROOT)/os/$(LINUX_DIR)/ O=$(JMKE_BUILDDIR)/os/$(LINUX_DIR)/ prepare

ifdef CONFIG_RG_OS_LINUX
del_subdirs:
	rm -rf $(STATIC_MOD_DIR)
endif

.PHONY: cp_kernel create_links modules del_subdirs prepare_linux kernel_ramdisk kernel_ramdisk_prepare

INSTALL_MOD_PATH := $(JMKE_BUILDDIR)/os/modules

kernel_ramdisk_prepare:
	rm -rf $(INSTALL_MOD_PATH)
	$(MKDIR) $(INSTALL_MOD_PATH)
	$(MAKE) -C $(LINUX_DIR) INSTALL_MOD_PATH=$(INSTALL_MOD_PATH) modules_install

kernel_ramdisk: kernel_ramdisk_prepare
	$(call JMKF_ADD_RAMDISK_KERNEL_MODULES,$(shell find $(INSTALL_MOD_PATH) -name "*.ko"))
