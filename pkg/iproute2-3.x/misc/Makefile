ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JMK_LDFLAGS+=-lm

JMK_TARGET=nstat ifstat rtacct lnstat ss

JMK_INTERNAL_HEADERS+=ssfilter.h lnstat.h  

JMK_LIBS=$(LIBNETLINK)

JMK_O_OBJS_lnstat=lnstat.o lnstat_util.o
JMK_O_OBJS_nstat=nstat.o
JMK_O_OBJS_ifstat=ifstat.o
JMK_O_OBJS_rtacct=rtacct.o
JMK_O_OBJS_ss=ss.o ssfilter.o

JMK_AUTOGEN_SRC=ssfilter.c

$(JMKE_PWD_BUILD)/ssfilter.c: $(JMKE_PWD_BUILD)/ssfilter.y
	$(YACC) -o $@ $<

JMK_CLEAN+=$(JMK_AUTOGEN_SRC)

JMK_RAMDISK_BIN_FILES+=$(JMK_TARGET)

$(call JMKE_INCLUDE_RULES)
