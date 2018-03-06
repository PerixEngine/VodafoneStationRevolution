# This file is to workaround B46276, remove it when properly fixed!
ifdef CONFIG_RG_GPL
  ifdef CONFIG_RG_FILESERVER_ACLS
    JMK_ARCHCONFIG_FIRST_TASKS+=$(if $(wildcard $(JMK_ROOT)/pkg/acl),,libc_config.h) 
  endif
  ifdef CONFIG_RG_LPD
    ifeq ($(wildcard $(JMK_ROOT)/pkg/lpd),) 
      JMK_ARCHCONFIG_FIRST_TASKS+=libc_config.h
      CONFIG_RG_LPD=
    endif
  endif

libc_config.h:
ifdef JMKE_IS_BUILDDIR
	cp $(JMKE_BUILDDIR)/pkg/include/libc_config.h $@
	$(if $(wildcard $(JMK_ROOT)/pkg/acl),,echo "#undef CONFIG_RG_FILESERVER_ACLS" >> $@)
	$(if $(wildcard $(JMK_ROOT)/pkg/lpd),,echo "#undef CONFIG_RG_LPD" >> $@)
endif

endif
