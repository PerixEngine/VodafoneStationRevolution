ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../../
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JMK_EXPORT_HEADERS_DIR=sigc++/adaptors/lambda
JMK_EXPORT_HEADERS=base.h group.h lambda.h operator.h select.h

JMK_O_TARGET=lambda_lib.o
JMK_O_OBJS_$(JMK_O_TARGET)+=lambda.o

$(call JMKE_INCLUDE_RULES)
