ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../../
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JMK_INTERNAL_HEADERS+=ttdriver.h tterrors.h ttgload.h ttgxvar.h ttinterp.h \
  ttobjs.h ttpic.h ttpload.h ttpic.c ttdriver.c ttpload.c ttgload.c ttobjs.c \
  ttinterp.c ttgxvar.c

JMK_O_TARGET=truetype_lib.o
JMK_O_OBJS_$(JMK_O_TARGET)+=truetype.o

$(call JMKE_INCLUDE_RULES)
