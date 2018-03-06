ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../..
  include $(JMK_ROOT)/jmk/env_root.mak
endif
include $(JMK_ROOT)/pkg/voip/asterisk/common.mak

JMK_SUBDIRS=btree db hash mpool recno

JMK_A_TARGET=libdb1.a
JMK_O_OBJS_libdb1.a=btree/btree_target.o db/db_target.o hash/hash_target.o \
  mpool/mpool_target.o recno/recno_target.o

$(call JMKE_INCLUDE_RULES)
