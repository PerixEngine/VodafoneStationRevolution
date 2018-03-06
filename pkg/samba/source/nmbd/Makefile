SMBDIR=..
include $(SMBDIR)/envir.mak

JMK_TARGET=nmbd
JMK_RAMDISK_BIN_FILES+=$(JMK_TARGET)
JMK_O_OBJS=asyncdns.o nmbd_become_dmb.o nmbd_become_lmb.o nmbd_browserdb.o \
  nmbd_browsesync.o nmbd_elections.o nmbd_incomingdgrams.o \
  nmbd_incomingrequests.o nmbd_lmhosts.o nmbd_logonnames.o \
  nmbd_mynames.o nmbd_namelistdb.o nmbd_namequery.o nmbd_nameregister.o \
  nmbd_namerelease.o nmbd_nodestatus.o nmbd.o nmbd_packets.o \
  nmbd_processlogon.o nmbd_responserecordsdb.o nmbd_sendannounce.o \
  nmbd_serverlistdb.o nmbd_subnetdb.o nmbd_synclists.o nmbd_winsproxy.o \
  nmbd_winsserver.o nmbd_workgroupdb.o ../lib/dummysmbd.o 

ifeq ($(call JMKF_GCC_VER_GE,"4003"),y)
  JMK_CFLAGS_nmbd_packets.o+=-Wno-array-bounds
endif

JMK_LDLIBS:=$(JMK_LDLIBS) -lmgt_client

include $(SMBMK)

