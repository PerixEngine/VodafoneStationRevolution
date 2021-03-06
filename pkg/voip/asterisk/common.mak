# If you don't have a lot of memory (e.g. embedded Asterisk), uncomment the
# following to reduce the size of certain static buffers
#OPTIONS += -DLOW_MEMORY

# Optional debugging parameters
DEBUG_THREADS = #-DDEBUG_THREADS #-DDO_CRASH 

# Uncomment next one to enable ast_frame tracing (for debugging)
TRACE_FRAMES = #-DTRACE_FRAMES

# Uncomment next one to enable malloc debugging
# You can view malloc debugging with:
#   *CLI> show memory allocations [filename]
#   *CLI> show memory summary [filename]
#
MALLOC_DEBUG = #-include $(PWD)/include/asterisk/astmm.h

# Where to install asterisk after compiling
# Default -> leave empty
INSTALL_PREFIX=

# Original busydetect routine
BUSYDETECT = #-DBUSYDETECT

# Improved busydetect routine, comment the previous one if you use this one
BUSYDETECT+= -DBUSYDETECT_MARTIN 
# Detect the busy signal looking only at tone lengths
# For example if you have 3 beeps 100ms tone, 100ms silence separated by 500 ms of silence
BUSYDETECT+= #-DBUSYDETECT_TONEONLY
# Inforce the detection of busy singal (get rid of false hangups)
# Don't use together with -DBUSYDETECT_TONEONLY
BUSYDETECT+= #-DBUSYDETECT_COMPARE_TONE_AND_SILENCE

JMK_CFLAGS+=-I$(JMKE_BUILDDIR)/pkg/voip/asterisk/include
JMK_CFLAGS+=-DASTERISK_VERSION=\"$(ASTERISKVERSION)\"
JMK_CFLAGS+=-DINSTALL_PREFIX=\"$(INSTALL_PREFIX)\"
JMK_CFLAGS+=-DASTETCDIR=\"$(ASTETCDIR)\"
JMK_CFLAGS+=-DASTLIBDIR=\"$(ASTLIBDIR)\"
JMK_CFLAGS+=-DASTVARLIBDIR=\"$(ASTVARLIBDIR)\"
JMK_CFLAGS+=-DASTVARRUNDIR=\"$(ASTVARRUNDIR)\"
JMK_CFLAGS+=-DASTSPOOLDIR=\"$(ASTSPOOLDIR)\"
JMK_CFLAGS+=-DASTLOGDIR=\"$(ASTLOGDIR)\"
JMK_CFLAGS+=-DASTCONFPATH=\"$(ASTCONFPATH)\"
JMK_CFLAGS+=-DASTMODDIR=\"/lib/asterisk\"
JMK_CFLAGS+=-DASTAGIDIR=\"$(AGI_DIR)\"
JMK_CFLAGS+=$(DEBUG_THREADS)
JMK_CFLAGS+=$(TRACE_FRAMES)
JMK_CFLAGS+=$(MALLOC_DEBUG)
JMK_CFLAGS+=$(BUSYDETECT)
JMK_CFLAGS+=$(OPTIONS)
JMK_CFLAGS+=-D_GNU_SOURCE

ASTLIBDIR=$(INSTALL_PREFIX)/usr/lib/asterisk
ASTVARLIBDIR=$(INSTALL_PREFIX)/var/lib/asterisk
ASTETCDIR=$(INSTALL_PREFIX)/etc/asterisk
ASTSPOOLDIR=$(INSTALL_PREFIX)/var/spool/asterisk
ASTLOGDIR=$(INSTALL_PREFIX)/var/log/asterisk
ASTHEADERDIR=$(INSTALL_PREFIX)/usr/include/asterisk
ASTCONFPATH=$(ASTETCDIR)/asterisk.conf
ASTBINDIR=$(INSTALL_PREFIX)/usr/bin
ASTSBINDIR=$(INSTALL_PREFIX)/usr/sbin
ASTVARRUNDIR=$(INSTALL_PREFIX)/var/run
ASTMANDIR=$(INSTALL_PREFIX)/usr/share/man

JMK_SO_CFLAGS+=-Xlinker -x
