SMBDIR=..
include $(SMBDIR)/envir.mak

JMK_TARGET+=smbclient
JMK_TARGET+=$(if $(CONFIG_SMB_FS),smbmount smbumount smbmnt)
JMK_TARGET+=$(if $(CONFIG_CIFS),mount.cifs)
JMK_RAMDISK_BIN_FILES+=$(JMK_TARGET)

# We whould like to have 2 targets: smbclient and smcp. Howerver, the
# size of each executable is big. Therefore, we hold one executable that can
# run each one of them.
JMK_RAMDISK_LAST_TASKS+=smbclient_ramdisk
smbclient_ramdisk:
	$(JMKE_LN) $(JMKE_MAINFS_RUNTIME_MOUNT_POINT)/bin/smbclient \
	  $(JMKE_RAMDISK_RW_DIR)/bin/smbcp

JMK_O_OBJS_smbclient=client.o clitar.o write_all.o smbcp.o main.o

ifeq ($(call JMKF_GCC_VER_GE,"4003"),y)
  JMK_CFLAGS_clitar.o+=-Wno-array-bounds
endif

JMK_O_OBJS_smbmount=smbmount.o
JMK_O_OBJS_smbmnt=smbmnt.o
JMK_O_OBJS_smbumount=smbumount.o
JMK_O_OBJS_mount.cifs=mount.cifs.o

JMK_INTERNAL_HEADERS+=write_all.h

JMK_LDLIBS+=-lresolv

include $(SMBMK)
