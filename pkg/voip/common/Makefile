JMK_ROOT=../../..
include $(JMK_ROOT)/envir.mak

JMK_EXPORT_HEADERS_DIR=voip/common
JMK_EXPORT_HEADERS+=voip_common_dsp.h

ifndef CONFIG_RG_GPL
  JMK_EXPORT_HEADERS+=voip_common_const.h hw_config.h voip_ipc_cmd.h 
endif

include $(JMKE_MK)
