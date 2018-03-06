__libc__links: $(JMKE_BUILDDIR)/pkg/build/gcc
ifneq ($(strip $(__pkg_libc)),)
	# Compiler wrapper will be built during LIBC compilation
	$(foreach d,$(__pkg_libc), \
	  $(JMKE_MKDIR) `dirname $(JMKE_BUILDDIR)/$d`; \
	  $(JMKE_CP_LN) $(JMK_ROOT)/$d $(JMKE_BUILDDIR)/$d)
endif

JMK_ARCHCONFIG_LAST_TASKS+= \
  $(if $(CONFIG_GLIBC),create_includes_os_glibc) \
  $(if $(CONFIG_ULIBC),create_includes_os_ulibc)

# Create pkg/build/include subdir. We need this to enable the include/linux 
# and include/asm to be differnt for each tree even though all use 
# /usr/local/openrg/<arch>/include subdir.
# Glibc support is only for i386 for ensure/mpatrol needs.
__create_includes: $(MKDIR_SUBDIRS) __libc__links
	# Preparing basic include files in include dir
ifdef CONFIG_GLIBC
    ifeq ($(GLIBC_IN_TOOLCHAIN),y)
	$(JMKE_CP_LN) $(LIBC_IN_TOOLCHAIN_PATH_SO)/* $(RG_LIB)
	$(JMKE_CP_LN) $(LIBC_IN_TOOLCHAIN_PATH_SO)/* $(DEBUG_PATH)
	$(JMKE_CP_LN) $(LIBC_IN_TOOLCHAIN_PATH_CRT)/* $(RG_LIB)
	$(JMKE_CP_LN) $(LIBC_IN_TOOLCHAIN_PATH_INC)/* $(RG_BUILD)/glibc/include
    endif
endif
ifdef CONFIG_ULIBC
    ifeq ($(ULIBC_IN_TOOLCHAIN),y)
	$(JMKE_CP_LN) $(LIBC_IN_TOOLCHAIN_PATH_SO)/* $(RG_LIB)
	$(JMKE_CP_LN) $(LIBC_IN_TOOLCHAIN_PATH_SO)/* $(DEBUG_PATH)
	$(JMKE_CP_LN) $(LIBC_IN_TOOLCHAIN_PATH_CRT)/* $(RG_LIB)
	$(JMKE_CP_LN) $(LIBC_IN_TOOLCHAIN_PATH_INC)/* $(RG_BUILD)/ulibc/include
    else
        # For non-toolchain ulibc libs are linked to $(RG_LIB) in pkg/ulibc/Makefile
	cp -f -R --symbolic-link $(JMKE_BUILDDIR)/$(ULIBC_DIR)/include/* $(RG_BUILD)/ulibc/include
        ifeq ($(CONFIG_RG_CXX),y)
	    $(JMKE_CP_LN) $(shell $(CC) -print-file-name=libgcc_s.so.1) $(RG_LIB)
        endif
    endif
endif
	# We want to use OpenRG linux kernel includes instead of default libc
	$(RM) -rf $(RG_BUILD)/[ug]libc/include/linux \
	  $(RG_BUILD)/[ug]libc/include/asm \
	  $(RG_BUILD)/[ug]libc/include/asm-generic \
	  $(RG_BUILD)/[ug]libc/include/scsi \
	  $(RG_BUILD)/[ug]libc/include/mtd

create_includes_os_%:
	# Linking linux headers to include dir
	$(JMKE_MKDIR) $(RG_BUILD)/$*/include
ifdef CONFIG_RG_OS_LINUX
ifdef CONFIG_RG_OS_LINUX_26
	$(JMKE_CP_LN) -L $(JMK_ROOT)/os/linux-2.6/include/linux $(RG_BUILD)/$*/include
	$(JMKE_CP_LN) -L $(JMKE_BUILDDIR)/os/linux-2.6/include/linux $(RG_BUILD)/$*/include
	$(JMKE_MKDIR) $(RG_BUILD)/$*/include/asm
	-$(JMKE_CP_LN) -L $(JMKE_BUILDDIR)/os/linux-2.6/include/asm-$(LIBC_ARCH)/* $(RG_BUILD)/$*/include/asm
	$(JMKE_CP_LN) -L $(JMK_ROOT)/os/linux-2.6/include/asm-$(LIBC_ARCH)/* $(RG_BUILD)/$*/include/asm
	$(JMKE_CP_LN) -L $(JMK_ROOT)/os/linux-2.6/include/asm-generic $(RG_BUILD)/$*/include
	$(JMKE_CP_LN) -L $(JMK_ROOT)/os/linux-2.6/include/scsi $(RG_BUILD)/$*/include
	$(JMKE_CP_LN) -L $(JMK_ROOT)/os/linux-2.6/include/mtd $(RG_BUILD)/$*/include
	$(JMKE_CP_LN) -L $(JMK_ROOT)/os/linux-2.6/include/net/bluetooth $(RG_BUILD)/$*/net
endif
ifdef CONFIG_RG_OS_LINUX_3X
	# Note the headers install for UML. We use ARCH=x86 to export headers, since after git 3be311e3
	# (and probably some others) several include files (sigcontext.h, unistd.h) are missing in arch/um.
	# When exporting with ARCH=um, libc misses system files and cannot be built.
	# A hint from the git log says "that of underlying architecture would get used" - so we build
	# the usermode with system files from x86 (since we build for X86_32 bit system).
	$(MAKE) $(if $(CONFIG_RG_UML),ARCH=x86) -C $(JMK_ROOT)/os/linux-3.x/ O=$(JMKE_BUILDDIR)/os/linux-3.x/ \
	  INSTALL_HDR_PATH=$(RG_BUILD)/kernel_headers headers_install
	$(JMKE_CP_LN) $(RG_BUILD)/kernel_headers/include $(RG_BUILD)/$*
endif
endif
