ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../../..
  include $(JMK_ROOT)/jmk/env_root.mak
endif
include $(JMK_ROOT)/pkg/voip/asterisk/db1-ast/common.mak

JMK_O_TARGET=hash_target.o
JMK_O_OBJS=hash.o hash_bigkey.o hash_buf.o hash_func.o hash_log2.o hash_page.o \
  hsearch.o ndbm.o

$(call JMKE_INCLUDE_RULES)
