ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../../
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JMK_CD_EXPORTED_FILES+=afangles.c afcjk.c afdummy.c afglobal.c afhints.c \
  afindic.c aflatin2.c aflatin.c afloader.c afmodule.c afpic.c afwarp.c

JMK_INTERNAL_HEADERS+=afangles.h afcjk.h afdummy.h aferrors.h afglobal.h \
  afhints.h afindic.h aflatin2.h aflatin.h afloader.h afmodule.h afpic.h \
  aftypes.h afwarp.h

JMK_O_TARGET=autofit_lib.o
JMK_O_OBJS_$(JMK_O_TARGET)+=autofit.o

$(call JMKE_INCLUDE_RULES)
