ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../
  include $(JMK_ROOT)/jmk/env_root.mak
endif

ifndef CONFIG_RG_GPL
  JMK_SUBDIRS=main

  JMK_O_TARGET=cwmp_ipc.o
  JMK_O_OBJS_$(JMK_O_TARGET)+=main/cwmp_ipc_main.o
endif

JMK_EXPORT_HEADERS_DIR=dslhome/ipc
JMK_EXPORT_HEADERS=client.h

JMK_LOCAL_A_TARGET=liblocal_cwmp_ipc_client.a
JMK_O_OBJS_$(JMK_LOCAL_A_TARGET)+=client.o

ifdef CONFIG_DYN_LINK
  LIB_IPC=libcwmp_ipc_client.so
  JMK_SO_TARGET=$(LIB_IPC)
  JMK_RAMDISK_LIB_FILES+=$(JMK_SO_TARGET)
else
  LIB_IPC=libcwmp_ipc_client.a
  JMK_A_TARGET=$(LIB_IPC)
endif
JMK_O_OBJS_$(LIB_IPC)+=client.o
JMK_EXPORT_LIBS+=$(LIB_IPC)

JMK_LOCAL_TARGET=sample
JMK_O_OBJS_$(JMK_LOCAL_TARGET)+=sample.o 
JMK_LIBS_$(JMK_LOCAL_TARGET)+= \
  $(JMKE_BUILDDIR)/pkg/dslhome/ipc/liblocal_cwmp_ipc_client.a
JMK_JPKG_EXPORTED_FILES+=sample.c

$(call JMKE_INCLUDE_RULES)
