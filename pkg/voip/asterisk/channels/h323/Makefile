ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../../..
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JMK_O_TARGET=target.o

JMK_CFLAGS+=-DNDEBUG -I$(JMKE_PWD_BUILD)/../../include \
  -fPIC -DP_USE_PRAGMA -D_REENTRANT \
  -I$(JMK_ROOT)/pkg/voip/pwlib/include \
  -I$(JMK_ROOT)/pkg/voip/openh323/include -Os -pipe

JMK_O_OBJS_$(JMK_O_TARGET)+=ast_h323.o

$(call JMKE_INCLUDE_RULES)
