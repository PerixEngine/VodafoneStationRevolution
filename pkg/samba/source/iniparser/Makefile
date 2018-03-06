#
# iniparser Makefile
#

# Compiler settings
CC      = gcc
JMK_CFLAGS  = -O3

# Ar settings to build the library
AR	    = ar
ARFLAGS = rcv

# Set RANLIB to ranlib on systems that require it (Sun OS < 4, Mac OSX)
# RANLIB  = ranlib
RANLIB = true

RM      = rm -f


# Implicit rules

SUFFIXES = .o .c .h .a .so .sl

COMPILE.c=$(CC) $(JMK_CFLAGS) -c
.c.o:
	@(echo "compiling $< ...")
	@($(COMPILE.c) -o $@ $<)


SRCS = src/iniparser.c \
	   src/dictionary.c \
	   src/strlib.c

OBJS = $(SRCS:.c=.o)


default:	libiniparser.a

libiniparser.a:	$(OBJS)
	@($(AR) $(ARFLAGS) libiniparser.a $(OBJS))
	@($(RANLIB) libiniparser.a)

clean:
	$(RM) $(OBJS)

veryclean:
	$(RM) $(OBJS) libiniparser.a
	rm -rf ./html ; mkdir html
	cd test ; $(MAKE) veryclean

docs:
	@(cd doc ; $(MAKE))
	
check:
	@(cd test ; $(MAKE))
