# makefile for gcc

# Uncomment the following if you need a specific compiler.
#CC=gcc

JMK_CFLAGS:=-O2 -Wall -L../
JMK_LDFLAGS:=-s
LIBS:=-lm

all: ding model lding lmodel corr

corr: corr.c
	$(CC) $(JMK_CFLAGS) $(JMK_LDFLAGS) -o $@ $<

ding: ding.c
	$(CC) $(JMK_CFLAGS) $(JMK_LDFLAGS) -o $@ $< $(LIBS)

model: model.c
	$(CC) $(JMK_CFLAGS) $(JMK_LDFLAGS) -o $@ $< $(LIBS)

lding: ding.c
	$(CC) $(JMK_CFLAGS) -DLSAMPL $(JMK_LDFLAGS) -o $@ $< $(LIBS)

lmodel: model.c
	$(CC) $(JMK_CFLAGS) -DLSAMPL $(JMK_LDFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f ding lding model lmodel corr *~

