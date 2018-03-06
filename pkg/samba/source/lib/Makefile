SMBDIR=..
include $(SMBDIR)/envir.mak

JMK_A_TARGET=libsmbutil.a
JMK_O_OBJS=access.o bitmap.o charcnv.o crc32.o debug.o fault.o \
  fsusage.o genrand.o interface.o interfaces.o \
  md4.o messages.o ms_fnmatch.o pam_errors.o pidfile.o \
  readline.o select.o sendfile.o signal.o smbrun.o \
  substitute.o sysacls.o system.o time.o ufc.o username.o \
  util_file.o util.o util_seaccess.o util_sec.o util_sid.o \
  util_sock.o util_str.o util_unistr.o wins_srv.o \
  version.o util_pw.o dprintf.o xfile.o clobber.o util_uuid.o \
  data_blob.o sock_exec.o tallocmsg.o dmallocmsg.o md5.o hmacmd5.o iconv.o \
  account_pol.o adt_tree.o gencache.o module.o ldap_escape.o \
  privileges.o secdesc.o secace.o secacl.o popt_common.o \
  server_mutex.o system_smbd.o audit.o display_sec.o events.o \
  repdir.o sharesec.o util_nscd.o ../intl/lang_tdb.o dummysmbd.o \
  dummyroot.o util_tdb.o replace/getpass.o replace/timegm.o replace/replace.o \
  talloc/talloc.o

JMK_CFLAGS+=-Wno-format -D_SAMBA_BUILD_ -DCONFIGFILE=\"$(CONFIGFILE)\"

include $(SMBMK)
