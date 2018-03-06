JMK_ROOT=../../
include $(JMK_ROOT)/envir.mak

JMK_EXPORT_HEADERS_DIR=l2tp
JMK_EXPORT_HEADERS+=rg_ipc.h l2tp.h

VERSION=0.3

LIB_EVENT=libevent
JMK_SUBDIRS=$(LIB_EVENT) mgt

JMK_TARGET=l2tpd
JMK_RAMDISK_BIN_FILES+=$(JMK_TARGET)
JMK_O_OBJS_l2tpd=auth.o debug.o dgram.o main.o md5.o network.o options.o peer.o session.o tunnel.o utils.o rg.o 

JMK_CFLAGS_l2tpd=-DVERSION=\"$(VERSION)\"
JMK_LDFLAGS_l2tpd=-L$(LIB_EVENT) 
JMK_LDLIBS_l2tpd:=-levent -lutil $(OPENRG_GPL_LDLIBS) $(JMK_LDLIBS_l2tpd)

JMK_INTERNAL_HEADERS+=md5.h

include $(JMKE_MK)

