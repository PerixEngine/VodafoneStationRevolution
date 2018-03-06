# C compiler
#CC=cc
CC=gcc

# Uncomment the following to add symbols to the code for debugging
#DEBUG=-g -Wall

# Optimization for the compiler
#OPTIMIZE=
OPTIMIZE=-O2

JMK_CFLAGS= $(DEBUG) $(OPTIMIZE)

OBJS = crackcheck.o
LIBS = -lcrack

crackcheck: $(OBJS)
	$(CC) $(JMK_CFLAGS) $(LIBS) -o crackcheck $(OBJS)

clean:
	rm -f core *.o crackcheck

install: crackcheck
	install -m 555 crackcheck $(PREFIX)/sbin/crackcheck

