JMK_ROOT=../../..
include $(JMK_ROOT)/envir.mak

JMK_TARGET=excepttest algotest bitsettest dequetest fstreamtest  \
  listtest maptest mmaptest numerictest settest sstreamtest \
  stacktest streambuftest streamitertest stringtest vectortest \
  combotest ioexceptiontest iotest newdeltest 

$(foreach t, $(JMK_TARGET), $(eval JMK_O_OBJS_$t:=$(t:%=%.o) testframework.o))

JMK_LDFLAGS+=-L$(JMKE_BUILDDIR)/pkg/uclibc++/src -luClibc++ -ldl -lgcc_s

JMK_CFLAGS=-I../include -I$(JMKE_BUILDDIR) -g

JMK_INTERNAL_HEADERS+=testframework.h

include $(JMKE_MK)
