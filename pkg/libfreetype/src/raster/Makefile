ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../../
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JMK_INTERNAL_HEADERS+=ftmisc.h ftraster.h ftrend1.h rasterrs.h rastpic.h \
  ftraster.c ftrend1.c rastpic.c

JMK_O_TARGET=raster_lib.o
JMK_O_OBJS_$(JMK_O_TARGET)+=raster.o

$(call JMKE_INCLUDE_RULES)
