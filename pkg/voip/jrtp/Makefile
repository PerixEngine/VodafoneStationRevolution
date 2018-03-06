JMK_ROOT=../../../
include $(JMK_ROOT)/envir.mak

JMK_EXPORT_HEADERS_DIR=rtp
JMK_EXPORT_HEADERS+=rtp.h

ifndef CONFIG_RG_GPL
JMK_INTERNAL_HEADERS+=rtp_p.h
JMK_MOD_TARGET=rtp.o
JMK_RAMDISK_MODULES_FILES+=$(JMK_MOD_TARGET)
JMK_OX_OBJS_rtp.o=rtp_dev.o
JMK_O_OBJS_rtp.o=net.o ioctl.o rtp_prot.o rtcp_prot.o

ifeq ($(CONFIG_RG_JRTP),y)
  JMK_MOD_2_STAT+=$(JMK_MOD_TARGET)
endif
endif

include $(JMKE_MK)
