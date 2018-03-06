ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../../..
  include $(JMK_ROOT)/jmk/env_root.mak
endif
include $(JMK_ROOT)/pkg/voip/asterisk/db1-ast/common.mak

JMK_O_TARGET=btree_target.o
JMK_O_OBJS=bt_close.o bt_conv.o bt_debug.o bt_delete.o bt_get.o bt_open.o \
  bt_overflow.o bt_page.o bt_put.o bt_search.o bt_seq.o bt_split.o bt_utils.o

JMK_CFLAGS+=-I$(JMK_ROOT)/pkg/voip/asterisk/db1-ast/include

$(call JMKE_INCLUDE_RULES)
