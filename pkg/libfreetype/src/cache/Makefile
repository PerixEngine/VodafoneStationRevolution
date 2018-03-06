ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../../
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JMK_INTERNAL_HEADERS+=ftccache.h ftccback.h ftcerror.h ftcglyph.h ftcimage.h \
  ftcmanag.h ftcmru.h ftcsbits.h ftcbasic.c ftccache.c ftccmap.c ftcglyph.c \
  ftcimage.c ftcmanag.c ftcmru.c ftcsbits.c

JMK_O_TARGET=cache.o
JMK_O_OBJS_$(JMK_O_TARGET)+=ftcache.o

$(call JMKE_INCLUDE_RULES)
