#
# Asterisk -- A telephony toolkit for Linux.
# 
# Various utilities
#
# Copyright (C) 1999-2005, Digium
#
# Mark Spencer <markster@digium.com>
#
# This program is free software, distributed under the terms of
# the GNU General Public License
#

#
# Don't use ast mm routines
#
JMK_CFLAGS+=-DNO_AST_MM

ifeq ($(findstring BSD,${OSARCH}),BSD)
  JMK_CFLAGS+=-I$(CROSS_COMPILE_TARGET)/usr/local/include -L$(CROSS_COMPILE_TARGET)/usr/local/lib
endif

JMK_TARGET=stereorize streamplayer

ifneq ($(wildcard $(CROSS_COMPILE_TARGET)/usr/include/popt.h)$(wildcard -f $(CROSS_COMPILE_TARGET)/usr/local/include/popt.h),)
  JMK_TARGET+=smsq
endif

ifneq ($(wildcard $(CROSS_COMPILE_TARGET)/usr/include/newt.h)$(wildcard -f $(CROSS_COMPILE_TARGET)/usr/local/include/newt.h),)
  JMK_TARGET+=astman
endif

ifeq (${OSARCH},SunOS)
  SOL=../strcompat.o
  SOLLIBS=-lsocket -lnsl
endif

all: depend $(JMK_TARGET)

install:
	for x in $(JMK_TARGET); do \
		if [ "$$x" != "none" ]; then \
			$(INSTALL) -m 755 $$x $(DESTDIR)$(ASTSBINDIR)/$$x; \
		fi; \
	done 

clean:
	rm -f *.o astman smsq stereorize streamplayer check_expr .depend
	rm -f ast_expr2.o ast_expr2f.o

astman: astman.o ../md5.o
	$(CC) $(JMK_CFLAGS) -o astman astman.o ../md5.o -lnewt

stereorize: stereorize.o frame.o
	$(CC) $(JMK_CFLAGS) -o stereorize stereorize.o frame.o -lm

ast_expr2.o: ../ast_expr2.c
	gcc -g -c -o $@ $<

ast_expr2f.o: ../ast_expr2f.c
	gcc -g -c -DSTANDALONE -o $@ $<

check_expr: check_expr.c ast_expr2.o ast_expr2f.o
	$(CC) $(JMK_CFLAGS) -o $@ $^

smsq: smsq.o
	$(CC) $(JMK_CFLAGS) -o smsq ${SOL} smsq.o -lpopt

streamplayer: streamplayer.o
	$(CC) $(JMK_CFLAGS) -o streamplayer ${SOL} streamplayer.o ${SOLLIBS}

ifneq ($(wildcard .depend),)
  include .depend
endif

depend: .depend

.depend:
	../build_tools/mkdep $(JMK_CFLAGS) `ls *.c`

