ifndef JMKE_ENV_INCLUDED
JMK_ROOT=../../../../..
include $(JMK_ROOT)/jmk/env_root.mak
endif
include $(JMK_ROOT)/pkg/voip/asterisk/common.mak

JMK_O_TARGET=channel_capi.o

JMK_CFLAGS+=-I./libcapi20
JMK_CFLAGS+=-DCRYPTO -DCAPI_LIBRARY_V2
JMK_CFLAGS+=-DT38_SUPPORT
JMK_CFLAGS_chan_capi.o+=-Dload_module=channel_capi_load_module \
  -Dunload_module=channel_capi_unload_module \
  -Dreload=channel_capi_reload \
  -Ddescription=channel_capi_description \
  -Dkey=channel_capi_key \
  -Dusecount=channel_capi_usecount

JMK_RAMDISK_ETC_FILES+=capi20.conf

JMK_INTERNAL_HEADERS+=chan_capi_platform.h chan_capi20.h \
	chan_capi.h chan_capi_platform.h chan_capi_rtp.h \
	chan_capi_supplementary.h chan_capi_utils.h dlist.h xlaw.h

ifdef CHAN_CAPI_ADVANCED_FEATURES
JMK_INTERNAL_HEADERS+= chan_capi_chat.h chan_capi_command.h \
	chan_capi_qsig_asn197ade.h chan_capi_qsig_asn197no.h \
	chan_capi_qsig_ecma.h chan_capi_qsig.h
endif

JMK_INTERNAL_HEADERS+=libcapi20/capi20.h libcapi20/capi20_platform.h \
	libcapi20/capicmd.h libcapi20/capiutils.h

JMK_O_OBJS+=chan_capi.o chan_capi_utils.o chan_capi_rtp.o xlaw.o dlist.o \
	chan_capi_supplementary.o

ifdef CHAN_CAPI_ADVANCED_FEATURES
JMK_O_OBJS+=chan_capi_command.o chan_capi_qsig_core.o chan_capi_qsig_ecma.o \
	chan_capi_qsig_asn197ade.o chan_capi_qsig_asn197no.o chan_capi_chat.o
endif

JMK_SUBDIRS+=libcapi20

JMK_O_OBJS+=libcapi20/convert.o libcapi20/capi20.o libcapi20/capifunc.o

$(call JMKE_INCLUDE_RULES)
