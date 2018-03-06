SMBDIR=..
include $(SMBDIR)/envir.mak

JMK_A_TARGET=libsmb.a
JMK_O_OBJS=cliconnect.o clidgram.o clientgen.o clierror.o clifile.o \
  clilist.o climessage.o clioplock.o cliprint.o clirap.o clireadwrite.o \
  clisecdesc.o clistr.o clitrans.o credentials.o doserr.o errormap.o \
  namecache.o namequery.o nmblib.o nterr.o passchange.o pwd_cache.o \
  smbdes.o smbencrypt.o smberr.o ../groupdb/mapping.o ../groupdb/mapping_tdb.o \
  unexpected.o clikrb5.o clispnego.o spnego.o asn1.o cliquota.o clifsinfo.o clirap2.o \
  ntlmssp.o ntlmssp_parse.o ntlmssp_sign.o conncache.o smb_signing.o \
  namequery_dc.o trustdom_cache.o trusts_util.o samlogon_cache.o ntlm_check.o \
  clidfs.o dcerpc_err.o ../lib/arc4.o

ifeq ($(call JMKF_GCC_VER_GE,"4003"),y)
  JMK_CFLAGS_clidgram.o+=-Wno-array-bounds
endif

#libsmbclient is also built, but it is not clear why do they need it

include $(SMBMK)
