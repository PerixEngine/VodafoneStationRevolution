ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../
  include $(JMK_ROOT)/jmk/env_root.mak
endif

ifndef CONFIG_RG_GPL
JMK_SUBDIRS=main

JMK_A_TARGET=libcwmp.a
JMK_O_OBJS=cwmp_session.o cwmp_soap_encoder.o cwmp_rpc_methods.o cwmp_params.o \
  cwmp_params_data_model_dump.o $(JMKE_O_LANG_FILES)

JMK_LOCAL_A_TARGET=liblocal_cwmp.a
JMK_O_OBJS_$(JMK_LOCAL_A_TARGET)=$(JMK_O_OBJS)
JMK_EXPORT_LIBS=$(JMK_LOCAL_A_TARGET)

# headers are needed in jnet
JMK_EXPORT_HEADERS_DIR=dslhome
JMK_EXPORT_HEADERS=cwmp_soap_strings.h cwmp_session.h cwmp_params.h \
  cwmp_soap_encoder.h

JMK_INTERNAL_HEADERS=cwmp_rpc_methods.h test_util.h test_stubs.h \
  cwmp_params_data_model_dump.h

ifdef CONFIG_OPENRG
  JMK_LOCAL_TARGET=local_test
  JMK_O_OBJS_$(JMK_LOCAL_TARGET)=$(JMK_O_OBJS) test.o test_util.o
  JMK_L_OBJS_$(JMK_LOCAL_TARGET)+=main/mt_cwmp_params.o \
				  main/mt_cwmp_params_init.o
  JMK_LIBS_$(JMK_LOCAL_TARGET)=$(JMKE_BUILDDIR)/pkg/xml/liblocal_xml.a \
    $(JMKE_BUILDDIR)/pkg/expat/liblocal_expat.a \
    $(JMKE_BUILDDIR)/pkg/util/liblocal_openrg.a \
    $(JMKE_BUILDDIR)/pkg/util/liblocal_jutil.a \
    $(JMKE_BUILDDIR)/pkg/mgt/lib/liblocal_mgt.a \
    $(JMKE_BUILDDIR)/pkg/openssl/crypto/liblocal_crypto.a \
    $(JMKE_BUILDDIR)/pkg/util/liblocal_rg_config.a

  # Always needed to export for jpkg
  JMK_LOCAL_O_TARGET=local_test_stubs_.o
  JMK_O_OBJS_$(JMK_LOCAL_O_TARGET)=test_stubs.o

ifdef CONFIG_RG_DSLHOME_VOUCHERS
  JMK_L_OBJS_$(JMK_LOCAL_TARGET)+=main/mt_options.o
  JMK_LIBS_$(JMK_LOCAL_TARGET)+=$(JMKE_BUILDDIR)/pkg/mgt/lib/liblocal_mgt.a \
    $(JMKE_BUILDDIR)/pkg/openssl/ssl/liblocal_ssl.a \
    $(JMKE_BUILDDIR)/pkg/xmlsec/src/openssl/liblocal_xmlsec1_openssl.a \
    $(JMKE_BUILDDIR)/pkg/openssl/crypto/liblocal_crypto.a \
    $(JMKE_BUILDDIR)/pkg/xmlsec/src/openssl/liblocal_xmlsec1_openssl.a \
    $(JMKE_BUILDDIR)/pkg/libxml/liblocal_xml2.a __local_m
else
  JMK_L_OBJS_$(JMK_LOCAL_TARGET)+=test_stubs.o
endif

  JMK_L_OBJS_$(JMK_A_TARGET)+=main/cwmp_main.o

  JMK_RUN_UNITTEST+=test
  JMK_JPKG_EXPORTED_DIR+=test_files
  JMK_LINK_DIRS+=test_files
endif

ifneq ($(CONFIG_RG_WBM),)
  JMK_SUBDIRS+=wbm
  JMK_JPKG_EXPORTED_DIR+=wbm
  JMK_L_OBJS_$(JMK_A_TARGET)+=wbm/wbm.o
endif

ifneq ($(CONFIG_RG_TR_098_QOS),)
  JMK_SUBDIRS+=TR-098
  JMK_L_OBJS_$(JMK_A_TARGET)+=TR-098/cwmp_tr_098.o
endif

ifneq ($(CONFIG_RG_TR_140),)
  JMK_SUBDIRS+=TR-140
  JMK_L_OBJS_$(JMK_A_TARGET)+=TR-140/cwmp_tr_140.o
endif
endif

ifneq ($(CONFIG_RG_CWMP_IPC),)
  JMK_SUBDIRS+=ipc
  JMK_L_OBJS_$(JMK_A_TARGET)+=ipc/cwmp_ipc.o
endif

ifndef CONFIG_RG_GPL
JMK_SUBDIRS+=TR-106 TR-111 data_model
JMK_L_OBJS_$(JMK_A_TARGET)+=TR-106/cwmp_tr_106.o TR-111/cwmp_tr_111.o \
  data_model/data_model.o

JMK_ARCHCONFIG_FIRST_TASKS=test_file_links

ifdef CONFIG_OPENRG
  JMK_SUBDIRS+=jacs
endif
endif

$(call JMKE_INCLUDE_RULES)
