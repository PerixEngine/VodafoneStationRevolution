ifndef JMKE_ENV_INCLUDED
JMK_ROOT=../../../../../..
include $(JMK_ROOT)/jmk/env_root.mak
endif
include $(JMK_ROOT)/pkg/voip/asterisk/common.mak

JMK_O_TARGET+=converto.o capi20o.o capifunco.o

JMK_INTERNAL_HEADERS+=capi20.h capi20_platform.h capicmd.h capiutils.h

JMK_O_OBJS_converto.o=convert.o
JMK_O_OBJS_capi20o.o=capi20.o
JMK_O_OBJS_capifunco.o=capifunc.o

$(call JMKE_INCLUDE_RULES)
