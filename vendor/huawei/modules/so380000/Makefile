ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../..
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JMK_WARN2ERR=y

JMK_MOD_TARGET=so380000.o
JMK_O_OBJS_$(JMK_MOD_TARGET)+=input_so380000.o

include $(JMK_ROOT)/vendor/broadcom/envir.subdirs.mak
JMK_CFLAGS_$(JMK_MOD_TARGET)+= \
	-I$(BCM9636X_BUILDDIR)/bcmdrivers/opensource/include/bcm963xx/ \
	-I$(BCM9636X_BUILDDIR)/shared/opensource/include/bcm963xx/ \
	-I$(BCM9636X_BUILDDIR)/shared/broadcom/include/bcm963xx
JMK_RAMDISK_MODULES_FILES+=$(JMK_MOD_TARGET)

JMK_LOG_ENTITY_input_so380000.o=LOG_ENTITY_HW_BUTTONS

JMK_RAMDISK_TASKS+=ramdisk_chardev
ramdisk_chardev:
	$(JMKE_MKNOD) $(JMKE_RAMDISK_DEV_DIR)/input/event0 c 13 64

$(call JMKE_INCLUDE_RULES)
