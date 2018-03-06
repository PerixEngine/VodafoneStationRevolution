SMBDIR=..
include $(SMBDIR)/envir.mak

JMK_A_TARGET=libads.a
JMK_O_OBJS=kerberos.o ldap.o ldap_printer.o sasl.o krb5_setpw.o ldap_user.o \
  ads_struct.o ads_status.o disp_sec.o ads_utils.o ldap_utils.o \
  authdata.o util.o kerberos_verify.o cldap.o dns.o krb5_errs.o

include $(SMBMK)
