SMBDIR=..
include $(SMBDIR)/envir.mak

JMK_A_TARGET=libpassdb.a
JMK_O_OBJS=secrets.o pdb_get_set.o passdb.o pdb_interface.o machine_sid.o \
       pdb_compat.o lookup_sid.o pdb_smbpasswd.o pdb_tdb.o util_builtin.o \
       util_unixsids.o util_wellknown.o

include $(SMBMK)
