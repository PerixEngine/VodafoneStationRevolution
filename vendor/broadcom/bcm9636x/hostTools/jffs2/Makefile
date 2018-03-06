JMK_ROOT=../../../../..
include $(JMK_ROOT)/envir.mak

JMK_LOCAL_CFLAGS=-DWITHOUT_XATTR -I. -I$(JMK_ROOT)/pkg/include/zlib
JMK_LOCAL_CFLAGS+=-I../../../../../../os/linux-2.6/include
JMK_INTERNAL_HEADERS=compr.h crc32.h rbtree.h

JMK_LOCAL_TARGET+=mkfs.jffs2
JMK_O_OBJS_mkfs.jffs2+=crc32.o compr_rtime.o mkfs.jffs2.o compr_zlib.o compr.o \
  rbtree.o
JMK_LDLIBS_mkfs.jffs2+=-llocal_z

include $(JMKE_MK)
