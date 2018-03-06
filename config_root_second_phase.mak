MKDIR_SUBDIRS+=$(DEBUG_PATH) $(RG_LIB) $(RG_LIB)/include \
  $(if $(CONFIG_GLIBC),$(RG_BUILD)/glibc/include) \
  $(if $(CONFIG_ULIBC),$(RG_BUILD)/ulibc/include) \
  $(RG_BUILD)/local/include

ifeq ($(CONFIG_RG_JPKG_SRC)$(CONFIG_RG_BUILD_LOCAL_TARGETS_ONLY),)
  cfg_first_tasks: __create_includes
else
  cfg_first_tasks: $(MKDIR_SUBDIRS)
endif


# rg_gcc is created in pkg/build/Makefile and its depenancy (through gcc) is
# from create_includes.mak
include $(JMK_ROOT)/create_includes.mak

.PHONY: __libc__links
