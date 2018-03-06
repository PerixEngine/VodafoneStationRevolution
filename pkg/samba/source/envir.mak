JMK_ROOT=$(SMBDIR)/../../..
include $(JMK_ROOT)/envir.mak

#Directories
PREFIX=
BINDIR=$(PREFIX)/bin
SBINDIR=$(PREFIX)/bin
LIBDIR=$(PREFIX)/lib
VARDIR=$(PREFIX)/var
CONFIGDIR=$(PREFIX)/etc
MANDIR=$(PREFIX)/man
PRIVATEDIR=$(PREFIX)/etc
# This is where SWAT images and help files go
SWATDIR=$(PREFIX)/swat
# the directory where lock files go
LOCKDIR=$(VARDIR)/lock
# the directorty where pid files go
PIDDIR=$(VARDIR)/lock
LOGFILEBASE=$(VARDIR)/log
# The directory where code page definition files go
CODEPAGEDIR=$(LIBDIR)/codepages
#Files
SMB_PASSWD_FILE=$(PRIVATEDIR)/smbpasswd
TDB_PASSWD_FILE=$(PRIVATEDIR)/smbpasswd.tdb
PRIVATE_DIR = $(PRIVATEDIR)
DRIVERFILE=$(CONFIGDIR)/printers.def
CONFIGFILE=$(CONFIGDIR)/smb.conf
LMHOSTSFILE=$(CONFIGDIR)/lmhosts
# The current codepage definition list.
CODEPAGELIST=437 850 ISO8859-1
#CODEPAGELIST=437 737 775 850 852 861 932 866 949 950 936 1251 \
#  ISO8859-1 ISO8859-2 ISO8859-5 ISO8859-7 KOI8-R 857 ISO8859-9

# Not used by OpenRG but needs to be defined
PASSWD_PROGRAM=/usr/bin/passwd

INCDIRS=-I$(SMBDIR)/include -I$(SMBDIR) -I$(SMBDIR)/lib/replace \
	-I$(SMBDIR)/tdb/include -I$(SMBDIR)/librpc -I$(SMBDIR)/popt

ifdef CONFIG_RG_FILESERVER_ACLS
INCDIRS+=-I$(JMK_ROOT)/pkg/acl/include
endif

VARIABLES=PASSWD_PROGRAM SMB_PASSWD_FILE TDB_PASSWD_FILE LOGFILEBASE \
  CONFIGFILE LMHOSTSFILE SWATDIR SBINDIR LOCKDIR CODEPAGEDIR DRIVERFILE \
  BINDIR PIDDIR LIBDIR PRIVATE_DIR

# Add -Dxxx="$(xxx)"
DEFINES+=$(foreach v,$(VARIABLES),-D$v=\"$($v)\")

JMK_CFLAGS+=$(INCDIRS) -DHAVE_CONFIG_H
  
LIBS=$(SMBDIR)/locking/liblocking.a \
  $(SMBDIR)/printing/libprinting.a $(SMBDIR)/rpc_server/librpc_server.a \
  $(SMBDIR)/auth/libauth.a $(SMBDIR)/rpc_client/librpc_client.a \
  $(SMBDIR)/rpc_parse/librpc_parse.a $(SMBDIR)/passdb/libpassdb.a \
  $(SMBDIR)/param/libparam.a $(SMBDIR)/libsmb/libsmb.a \
  $(SMBDIR)/lib/libsmbutil.a $(SMBDIR)/passdb/libpassdb.a \
  $(SMBDIR)/tdb/libtdb.a $(SMBDIR)/rpc_parse/librpc_parse.a \
  $(SMBDIR)/libsmb/libsmb.a $(SMBDIR)/popt/libpopt.a \
  $(SMBDIR)/registry/libregistry.a $(SMBDIR)/lib/libsmbutil.a \
  $(SMBDIR)/rpc_client/librpc_client.a $(SMBDIR)/libads/libads.a \
  $(SMBDIR)/modules/libmodules.a $(SMBDIR)/services/libservices.a \
  $(SMBDIR)/librpc/librpc.a

# Add -L<path>
JMK_LDFLAGS+=$(foreach l,$(LIBS),-L$(dir $l))

JMK_LDLIBS:=-lnsl -lcrypt $(JMK_LDLIBS)

# Add -l<name>
JMK_LDLIBS:=$(foreach l,$(foreach l,$(LIBS),$(notdir $l)),-l$(l:lib%.a=%)) $(JMK_LDLIBS)

ifdef CONFIG_DYN_LINK
  JMK_LDLIBS:=-ldl $(JMK_LDLIBS)
endif

JMK_LDFLAGS+=$(SMBDIR)/dynconfig.o

SMBMK=$(SMBDIR)/samba.mak
