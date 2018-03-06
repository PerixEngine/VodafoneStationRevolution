ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JMK_SUBDIRS+=functors adaptors 

JMK_EXPORT_HEADERS_DIR=sigc++
JMK_EXPORT_HEADERS=bind.h bind_return.h class_slot.h connection.h hide.h \
  limit_reference.h method_slot.h object.h object_slot.h reference_wrapper.h \
  retype.h retype_return.h sigc++.h signal_base.h signal.h slot.h trackable.h \
  type_traits.h visit_each.h

JMK_SO_TARGET=libsigc-2.0.so
JMK_O_OBJS_$(JMK_SO_TARGET)+=signal.o signal_base.o trackable.o connection.o \
  $(JMKE_BUILDDIR)/pkg/libsigc++/sigc++/functors/functors.o \
  $(JMKE_BUILDDIR)/pkg/libsigc++/sigc++/adaptors/lambda/lambda_lib.o

JMK_EXPORT_LIBS+=$(JMK_SO_TARGET)
JMK_RAMDISK_LIB_FILES+=$(JMK_SO_TARGET)

$(call JMKE_INCLUDE_RULES)
