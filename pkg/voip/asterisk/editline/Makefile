ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../..
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JMK_A_TARGET=libeditline.a
JMK_O_OBJS_libeditline.a=editline.o history.o tokenizer.o readline.o np/np.o
JMK_CFLAGS+='-D__RCSID(x)=' '-D__RENAME(x)='

JMK_SUBDIRS+=np

$(call JMKE_INCLUDE_RULES)
