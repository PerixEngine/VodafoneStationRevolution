ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../../..
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JMK_O_TARGET=np.o
JMK_O_OBJS=fgetln.o strlcat.o strlcpy.o unvis.o vis.o
JMK_CFLAGS+=-I$(JMKE_PWD_BUILD)/.. '-D_DIAGASSERT(x)=' '-D__RENAME(x)='

$(call JMKE_INCLUDE_RULES)
