SMBDIR=..
include $(SMBDIR)/envir.mak

JMK_A_TARGET=libprinting.a
JMK_O_OBJS=load.o lpq_parse.o nt_printing.o pcap.o print_cups.o printfsp.o \
  print_generic.o printing.o print_svid.o notify.o printing_db.o

ifdef CONFIG_RG_LPD
  JMK_O_OBJS+=print_rg.o
endif

include $(SMBMK)
