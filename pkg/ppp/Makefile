ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JMK_SUBDIRS=pppd kernel
ifndef CONFIG_RG_GPL
JMK_A_TARGET=libppp.a
JMK_L_OBJS_$(JMK_A_TARGET)+=pppd/ppptask.o
JMK_L_OBJS_$(JMK_A_TARGET)+=pppd/plugins/plugins.o

ifneq ($(CONFIG_RG_PPTPS),)
  JMK_SUBDIRS+=pptp-server
  JMK_L_OBJS_$(JMK_A_TARGET)+=pptp-server/pptps_task.o
endif

ifneq ($(CONFIG_RG_L2TPS),)
  JMK_SUBDIRS+=l2tp-server
  JMK_L_OBJS_$(JMK_A_TARGET)+=l2tp-server/l2tps_task.o
endif

ifdef CONFIG_RG_PPPOES
  JMK_SUBDIRS+=pppoes
  JMK_L_OBJS_$(JMK_A_TARGET)+=pppoes/pppoes_task.o
endif

ifdef CONFIG_RG_PPPOS
  JMK_SUBDIRS+=pppos
  JMK_L_OBJS_$(JMK_A_TARGET)+=pppos/pppos_task.o
endif

ifdef CONFIG_RG_PPPOE
  JMK_SUBDIRS+=pppoe
  JMK_L_OBJS_$(JMK_A_TARGET)+=pppoe/pppoe.o
endif

ifdef CONFIG_RG_PPPOA
  JMK_SUBDIRS+=pppoa
  JMK_L_OBJS_$(JMK_A_TARGET)+=pppoa/pppoa.o
endif

ifdef CONFIG_RG_PPTPC
  JMK_SUBDIRS+=pptpc
  JMK_L_OBJS_$(JMK_A_TARGET)+=pptpc/pptpc.o
endif

ifdef CONFIG_RG_L2TPC
  JMK_SUBDIRS+=l2tpc
  JMK_L_OBJS_$(JMK_A_TARGET)+=l2tpc/l2tpc.o
endif

JMK_SUBDIRS+=mgt main

ifneq ($(CONFIG_RG_WBM),)
  JMK_SUBDIRS+=wbm
  JMK_JPKG_EXPORTED_DIR+=wbm
  JMK_L_OBJS_$(JMK_A_TARGET)+=wbm/wbm.o
endif

JMK_L_OBJS_$(JMK_A_TARGET)+=main/ppp_main.o
JMK_L_OBJS_$(JMK_A_TARGET)+=mgt/ppp_dev.o

JMK_SUBDIRS+=standalone

JMK_TARGET=ppp_main_standalone
JMK_RAMDISK_BIN_FILES+=$(JMK_TARGET)

JMK_L_OBJS_ppp_main_standalone+=standalone/main.o pppd/pppt.o \
  pppd/plugins/pppoe.o pppd/main.o pppd/lcp.o pppd/utils.o pppd/ppp_resource.o\
  pppd/options.o pppd/demand.o pppd/auth.o pppd/chap.o pppd/ipcp.o pppd/upap.o \
  pppd/ccp.o pppd/fsm.o pppd/sys-rg.o pppd/chap_ms.o pppd/mppe.o pppd/eui64.o \
  pppd/magic.o \
  $(JMKE_BUILDDIR)/pkg/util/rg_c_config.o

JMK_LIBS_ppp_main_standalone+=$(JMKE_BUILDDIR)/pkg/util/libjutil.so \
  $(JMKE_BUILDDIR)/pkg/util/libopenrg.so \
  $(JMKE_BUILDDIR)/pkg/openssl/ssl/libssl.so \
  $(JMKE_BUILDDIR)/pkg/openssl/crypto/libcrypto.so \
  $(JMKE_BUILDDIR)/pkg/lib/libz.so

ifdef CONFIG_RG_IPV6
  JMK_L_OBJS_ppp_main_standalone+=pppd/ipv6cp.o
endif

endif #CONFIG_RG_GPL

$(call JMKE_INCLUDE_RULES)
