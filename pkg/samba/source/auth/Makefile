SMBDIR=..
include $(SMBDIR)/envir.mak

JMK_A_TARGET=libauth.a
JMK_O_OBJS=auth.o auth_util.o auth_compat.o auth_ntlmssp.o pampass.o pass_check.o \
  auth_builtin.o auth_domain.o auth_sam.o auth_server.o auth_unix.o \
  auth_winbind.o

include $(SMBMK)
