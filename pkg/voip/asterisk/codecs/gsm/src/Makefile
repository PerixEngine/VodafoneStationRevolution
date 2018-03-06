ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../../../../
  include $(JMK_ROOT)/jmk/env_root.mak
endif
include $(JMK_ROOT)/pkg/voip/asterisk/common.mak

JMK_A_TARGET=libgsm.a

JMK_O_OBJS=add.o code.o debug.o decode.o gsm_create.o gsm_decode.o gsm_destroy.o \
  gsm_encode.o gsm_explode.o gsm_implode.o gsm_option.o gsm_print.o \
  long_term.o lpc.o preprocess.o rpe.o short_term.o table.o

JMK_CFLAGS+=-I(JMKE_PWD_BUILD)/../inc

$(call JMKE_INCLUDE_RULES)
