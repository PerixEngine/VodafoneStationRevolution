SMBDIR=..
include $(SMBDIR)/envir.mak

JMK_A_TARGET=libmodules.a
JMK_O_OBJS_libmodules.a=CP437.o CP850.o vfs_notify.o vfs_default.o

ifdef CONFIG_RG_FILESERVER_ACLS
  JMK_O_OBJS_libmodules.a+=vfs_posixacl.o
endif

include $(SMBMK)
