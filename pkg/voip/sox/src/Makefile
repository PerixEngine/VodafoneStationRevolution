JMK_ROOT=../../../..
include $(JMK_ROOT)/envir.mak

JMK_SUBDIRS+=gsm

JMK_A_TARGET=libst.a
JMK_O_OBJS_$(JMK_A_TARGET)=adpcm.o au.o auto.o g711.o g721.o g723_24.o \
  g723_40.o g72x.o gsm.o ima_rw.o mp3.o raw.o wav.o avg.o trim.o \
  getopt.o handlers.o misc.o resample.o stio.o util.o gsm/target.o
  
JMK_TARGET=sox
JMK_LDFLAGS_$(JMK_TARGET)=-L$(JMKE_BUILDDIR)/pkg/voip/sox/src -lst -lm
$(JMK_TARGET): $(JMK_A_TARGET)

ifdef CONFIG_RG_LIBMAD
  JMK_CFLAGS+=-DHAVE_LIBMAD
  JMK_LDFLAGS_$(JMK_TARGET)+=-L$(JMKE_BUILDDIR)/pkg/av/libmad -lmad
endif
ifdef CONFIG_RG_LAME
  JMK_CFLAGS+=-DHAVE_LAME
  JMK_LDFLAGS_$(JMK_TARGET)+=-L$(JMKE_BUILDDIR)/pkg/lame/libmp3lame -lmp3lame \
    -L$(JMKE_BUILDDIR)/pkg/lame/mpglib -lmpgdecoder
endif

JMK_O_OBJS_$(JMK_TARGET)=sox.o
JMK_RAMDISK_BIN_FILES+=$(JMK_TARGET)

include $(JMKE_MK)
