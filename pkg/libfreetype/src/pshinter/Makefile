ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../../
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JMK_INTERNAL_HEADERS+=pshalgo.h pshglob.h pshmod.h pshnterr.h pshpic.h \
  pshrec.h pshpic.c pshrec.c pshglob.c pshalgo.c pshmod.c

JMK_O_TARGET=pshinter_lib.o
JMK_O_OBJS_$(JMK_O_TARGET)+=pshinter.o

$(call JMKE_INCLUDE_RULES)
