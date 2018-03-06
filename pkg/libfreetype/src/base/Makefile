ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../../
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JMK_INTERNAL_HEADERS+=basepic.h ftbase.h ftpic.c basepic.c ftadvanc.c ftcalc.c\
  ftdbgmem.c ftgloadr.c ftobjs.c ftoutln.c ftrfork.c ftsnames.c ftstream.c \
  fttrigon.c ftutil.c ftmac.c

JMK_O_TARGET=base_lib.o
JMK_O_OBJS_$(JMK_O_TARGET)+=ftapi.o ftbase.o ftbbox.o ftbdf.o \
  ftbitmap.o ftcid.o ftdebug.o ftfstype.o ftgasp.o ftglyph.o \
  ftgxval.o ftinit.o ftlcdfil.o ftmm.o ftotval.o ftpatent.o ftpfr.o \
  ftstroke.o ftsynth.o ftsystem.o fttype1.o ftwinfnt.o ftxf86.o

$(call JMKE_INCLUDE_RULES)
