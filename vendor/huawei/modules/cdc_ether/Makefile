ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../..
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JMK_MOD_TARGET=huawei_cdc_ether_mod.o
JMK_O_OBJS_$(JMK_MOD_TARGET)+=hw_cdc_driver.o
JMK_CFLAGS_hw_cdc_driver.o+=$(if $(CONFIG_RG_HUAWEI_CDC_ETHER_NCM),\
  -DCONFIG_RG_HUAWEI_CDC_ETHER_NCM)
JMK_MOD_CFLAGS+=-I$(JMK_ROOT)/os/linux-2.6/drivers/usb/net

JMK_RAMDISK_MODULES_FILES+=$(JMK_MOD_TARGET)

JMK_RAMDISK_TASKS+=ramdisk_hw_cdc_ether_chrdev
ramdisk_hw_cdc_ether_chrdev:
	$(JMKE_MKNOD) $(JMKE_RAMDISK_DEV_DIR)/huawei_cdc_chrdev0 c 180 10

$(call JMKE_INCLUDE_RULES)
