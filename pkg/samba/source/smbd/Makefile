SMBDIR=..
include $(SMBDIR)/envir.mak

JMK_TARGET=smbd
JMK_RAMDISK_BIN_FILES+=$(JMK_TARGET)
JMK_O_OBJS=blocking.o chgpasswd.o close.o connection.o conn.o dfree.o dir.o \
  dosmode.o error.o fileio.o filename.o files.o ipc.o lanman.o \
  mangle_hash2.o mangle_hash.o mangle_map.o mangle.o message.o negprot.o \
  notify.o nttrans.o open.o \
  oplock_irix.o oplock_linux.o oplock.o password.o pipes.o posix_acls.o \
  process.o reply.o sec_ctx.o server.o service.o session.o \
  statcache.o trans2.o uid.o utmp.o vfs.o sesssetup.o srvstr.o \
  change_trust_pw.o fake_file.o quotas.o ntquotas.o build_options.o msdfs.o \
  aio.o statvfs.o dmapi.o map_username.o share_access.o notify_internal.o \
  notify_inotify.o ../nsswitch/wb_common.o ../nsswitch/wb_client.o

JMK_LDLIBS:=$(JMK_LDLIBS) -lmgt_client -lsmbutil -lrpc_server

ifdef CONFIG_RG_LPD
  # This should be removed when B19508 is fixed
  JMK_LDLIBS:=$(JMK_LDLIBS) -llpdc $(OPENRG_GPL_LDLIBS) 
endif

# Libraries for ACLs and Extended Attribute support
ifdef CONFIG_RG_FILESERVER_ACLS
  JMK_LDLIBS:=$(JMK_LDLIBS) -lacl -lmisc -lattr
endif

include $(SMBMK)

