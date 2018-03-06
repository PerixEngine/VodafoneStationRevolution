JMK_ROOT=../../../../../../..
include $(JMK_ROOT)/envir.mak

JMK_INTERNAL_HEADERS+=ledapi.h

JMK_EXPORT_HEADERS_DIR=vendor/broadcom/bcm9636x
JMK_EXPORT_HEADERS+=ledapi.h mcuctl_common.h

ifndef CONFIG_RG_GPL
JMK_INTERNAL_HEADERS+=fwupdate_cmd.h file_name.h

JMK_O_TARGET=mcu_update.o
JMK_CFLAGS_$(JMK_O_TARGET)=-I$(BCM_DIR)/userspace/public/include \
  -I$(BCM_DIR)/userspace/public/include/linux \
  -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD) \
  -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD)

JMK_O_OBJS_$(JMK_O_TARGET)=ledapi.o fwupdate_cmd.o
endif

include $(JMKE_MK)

