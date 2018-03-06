ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../../
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JMK_CFLAGS+=-I$(JMKE_PWD_BUILD)/../ -I$(JMKE_PWD_BUILD)/../../

JMK_INTERNAL_HEADERS+=linux_usbfs.h poll_posix.h threads_posix.h

JMK_O_TARGET=linux.o
JMK_O_OBJS+=linux_usbfs.o threads_posix.o

$(call JMKE_INCLUDE_RULES)
