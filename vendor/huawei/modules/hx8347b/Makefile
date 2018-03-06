ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../..
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JMK_MOD_O_TARGET=lcd_hw.o
JMK_O_OBJS_$(JMK_MOD_O_TARGET)=lcd_hw_hx8347b.o

include $(JMK_ROOT)/vendor/broadcom/envir.subdirs.mak
JMK_CFLAGS_lcd_hw.o+= \
  -I$(BCM9636X_BUILDDIR)/bcmdrivers/opensource/include/bcm963xx/ \
  -I$(BCM9636X_BUILDDIR)/shared/opensource/include/bcm963xx/ \
  -I$(BCM9636X_BUILDDIR)/shared/broadcom/include/bcm963xx

JMK_WARN2ERR=y

$(call JMKE_INCLUDE_RULES)
