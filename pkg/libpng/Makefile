ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JMK_EXPORT_HEADERS+=png.h pngconf.h
JMK_INTERNAL_HEADERS+=config.h pngpriv.h

ifdef CONFIG_DYN_LINK
  LIB_PNG=libpng.so
  JMK_SO_TARGET=$(LIB_PNG)
  JMK_RAMDISK_LIB_FILES+=$(LIB_PNG)
else
  LIB_PNG=libpng.a
  JMK_A_TARGET=$(LIB_PNG)
endif

JMK_O_OBJS_$(LIB_PNG)=png.o pngset.o pngget.o pngrutil.o pngtrans.o \
  pngwutil.o pngread.o pngrio.o pngwio.o pngwrite.o pngrtran.o pngwtran.o \
  pngmem.o pngerror.o pngpread.o
JMK_EXPORT_LIBS+=$(LIB_PNG)

JMK_CFLAGS+=-DPNG_CONFIGURE_LIBPNG -DHAVE_CONFIG_H

$(call JMKE_INCLUDE_RULES)
