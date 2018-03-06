ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JMK_INTERNAL_HEADERS+=localcharset.h

JMK_O_TARGET=libiconv.o
JMK_LOCAL_O_TARGET=local_libiconv.o
JMK_O_OBJS_$(JMK_O_TARGET)=iconv.o relocatable.o
JMK_O_OBJS_$(JMK_LOCAL_O_TARGET)=iconv.o relocatable.o

JMK_CFLAGS+=-I$(JMKE_PWD_BUILD)/..
JMK_LOCAL_CFLAGS+=-I$(JMKE_PWD_BUILD)/..

$(call JMKE_INCLUDE_RULES)
