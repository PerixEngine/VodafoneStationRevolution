CC=gcc
INCLUDES= -I`pwd` -I../../../source/ -I../../../source/include -I../../../source/ubiqx

DEFS= -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE

JMK_CFLAGS= -g -Wall -ansi $(INCLUDES) 

OBJ= util.o mgr_group.o mgr_user.o

JMK_LDFLAGS=-L. -L../../bin/ 
LIBS=../../../source/bin/libmsrpc.so

all: cacusermgr

cacusermgr: cacusermgr.o $(OBJ)
	$(CC) $(JMK_CFLAGS) $(JMK_LDFLAGS) -o $@ $< $(OBJ) $(LIBS)

.c.o:
	$(CC) $(JMK_CFLAGS) -c $< -o $@

clean:
	rm -f *.o cacusermgr
