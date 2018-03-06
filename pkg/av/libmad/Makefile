JMK_ROOT=../../..
include $(JMK_ROOT)/envir.mak

JMK_EXPORT_HEADERS+=mad.h

JMK_A_TARGET=libmad.a

JMK_O_OBJS_$(JMK_A_TARGET)+=fixed.o bit.o timer.o stream.o frame.o synth.o decoder.o \
  layer12.o layer3.o huffman.o
ifdef CONFIG_ARM
  JMK_O_OBJS_$(JMK_A_TARGET)+=imdct_l_arm.o
endif

JMK_CFLAGS+=-DHAVE_CONFIG_H

ifdef CONFIG_PPC
  JMK_CFLAGS+=-DFPM_PPC
endif
ifdef CONFIG_ARM
  JMK_CFLAGS+=-DFPM_ARM
endif
ifdef CONFIG_MIPS
  JMK_CFLAGS+=-DFPM_MIPS
endif
ifdef CONFIG_UM
  JMK_CFLAGS+=-DFPM_INTEL
endif

include $(JMKE_MK)
