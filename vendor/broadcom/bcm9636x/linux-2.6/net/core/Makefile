JMK_ROOT=../../../../../..
include $(JMK_ROOT)/envir.mak

JMK_MOD_TARGET+=nbuff.o blog.o
ifneq ($(strip $(CONFIG_BCM_BPM)),)
JMK_MOD_TARGET+=gbpm.o
JMK_O_OBJS_gbpm.o+=gbpm.o
endif
ifneq ($(strip $(CONFIG_BCM_INGQOS)),)
JMK_MOD_TARGET+=iqos.o
JMK_O_OBJS_iqos.o+=iqos.o
endif
JMK_MOD_2_STAT+=$(JMK_MOD_TARGET)
JMK_O_OBJS_nbuff.o+=nbuff.o
JMK_O_OBJS_blog.o+=blog.o

include $(JMKE_MK)	
