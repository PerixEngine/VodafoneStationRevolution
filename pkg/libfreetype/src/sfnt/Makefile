ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../../
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JMK_INTERNAL_HEADERS+=sfdriver.h sferrors.h sfntpic.h sfobjs.h ttbdf.h \
  ttcmapc.h ttcmap.h ttkern.h ttload.h ttmtx.h ttpost.h ttsbit.h sfntpic.c \
  ttload.c ttmtx.c ttcmap.c ttkern.c sfobjs.c sfdriver.c ttsbit.c ttpost.c \
  ttbdf.c ttsbit0.c

JMK_O_TARGET=sfnt_lib.o
JMK_O_OBJS_$(JMK_O_TARGET)+=sfnt.o

$(call JMKE_INCLUDE_RULES)
