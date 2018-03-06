ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../../
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JMK_INTERNAL_HEADERS+=ftgrays.h ftsmerrs.h ftsmooth.h ftspic.h ftgrays.c \
  ftsmooth.c ftspic.c

JMK_O_TARGET=smooth_lib.o
JMK_O_OBJS_$(JMK_O_TARGET)+=smooth.o

$(call JMKE_INCLUDE_RULES)
