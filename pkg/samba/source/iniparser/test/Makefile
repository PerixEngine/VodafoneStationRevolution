#
# iniparser tests Makefile
#

CC      = gcc
JMK_CFLAGS  = -g -I../src
LFLAGS  = -L.. -liniparser
AR	    = ar
ARFLAGS = rcv
RM      = rm -f


default: all

all: iniexample

iniexample: iniexample.c
	$(CC) $(JMK_CFLAGS) -o iniexample iniexample.c -I../src -L.. -liniparser

clean veryclean:
	$(RM) iniexample example.ini



