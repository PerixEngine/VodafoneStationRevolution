JMK_ROOT=../../
include $(JMK_ROOT)/envir.mak

MTD_CFLAGS += -I./include -D_FILE_OFFSET_BITS=64
JMK_CFLAGS += $(MTD_CFLAGS)

ifneq ($(CONFIG_MTD_UBI),)
  JMK_SUBDIRS=ubi-utils/src
endif

MTD_UTILS+=flash_erase flash_eraseall flash_info flashcp
ifneq ($(CONFIG_MTD_NAND)$(CONFIG_MTD_BRCMNAND),)
  MTD_UTILS+=nanddump nandwrite nandtest
  LOCAL_MTD_UTILS+=nandoob
endif

$(foreach t,$(MTD_UTILS) $(LOCAL_MTD_UTILS), \
  $(eval JMK_O_OBJS_$t+=$t.o))

JMK_O_OBJS_flash_eraseall+=crc32.o

JMK_CFLAGS_REMOVE_flashcp.o+=-Werror

JMK_TARGET+=$(MTD_UTILS)
JMK_RAMDISK_BIN_FILES+=$(JMK_TARGET)

JMK_LOCAL_TARGET+=$(LOCAL_MTD_UTILS)
JMK_LOCAL_CFLAGS+=$(MTD_CFLAGS)

JMK_INTERNAL_HEADERS+=common.h

include $(JMKE_MK)
