ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../../../
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JMK_EXPORT_HEADERS_DIR=sigc++/functors
JMK_EXPORT_HEADERS=functors.h functor_trait.h mem_fun.h ptr_fun.h slot_base.h \
  slot.h

JMK_O_TARGET=functors.o
JMK_O_OBJS_$(JMK_O_TARGET)+=slot.o slot_base.o 

$(call JMKE_INCLUDE_RULES)
