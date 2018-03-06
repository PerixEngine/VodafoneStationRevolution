ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JMK_SUBDIRS+=base truetype sfnt autofit pshinter raster smooth cache 

JMK_SO_TARGET=libfreetype.so
JMK_O_OBJS_$(JMK_SO_TARGET)+=base/base_lib.o truetype/truetype_lib.o \
  autofit/autofit_lib.o pshinter/pshinter_lib.o raster/raster_lib.o \
  smooth/smooth_lib.o sfnt/sfnt_lib.o
JMK_EXPORT_LIBS+=$(JMK_SO_TARGET)

$(call JMKE_INCLUDE_RULES)
