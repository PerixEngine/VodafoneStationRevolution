JMK_ROOT=../../../../../../../..
include $(JMK_ROOT)/envir.mak

JMK_MOD_TARGET+=spudd.o
JMK_O_OBJS_spudd.o += spudrv.o spuciphr.o spusha1.o spumd5.o spuipsec.o

JMK_MOD_CFLAGS += -I. -I$(INC_BRCMBOARDPARMS_PATH)/$(BRCM_BOARD)

JMK_RAMDISK_MODULES_FILES+=$(JMK_MOD_TARGET)
 
include $(JMKE_MK)
