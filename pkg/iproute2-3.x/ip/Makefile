ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JMK_LDFLAGS+=-lm -ldl

JMK_TARGET=ip rtmon
JMK_RAMDISK_BIN_FILES+=$(JMK_TARGET)

JMK_LIBS=$(LIBNETLINK)

JMK_INTERNAL_HEADERS+=xfrm.h ip_common.h tunnel.h

JMK_O_OBJS_ip=ip.o ipaddress.o ipaddrlabel.o iproute.o iprule.o \
  rtm_map.o iptunnel.o tunnel.o ipneigh.o ipntable.o iplink.o \
  ipmaddr.o ipmonitor.o ipmroute.o ipprefix.o iptuntap.o \
  ipxfrm.o xfrm_state.o xfrm_policy.o xfrm_monitor.o \
  iplink_vlan.o link_veth.o link_gre.o iplink_can.o \
  iplink_macvlan.o iplink_macvtap.o ipl2tp.o ipnetns.o ip6tunnel.o

ifeq ($(CONFIG_RG_IPV6),)
  JMK_O_OBJS_ip+=no_ip6.o
endif

JMK_O_OBJS_rtmon=rtmon.o

$(call JMKE_INCLUDE_RULES)
