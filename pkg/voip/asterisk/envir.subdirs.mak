# $1 - Source file
# $2 - Ramdisk destination dir
# Copy source file ($1) to ramdisk destination dir ($2)
Asterisk_Ramdisk=$(strip $1)__$(strip $2)/$(strip $1)

# $1 - Source file to be copied to /var/lib/asterisk in the ramdisk
Lib_Asterisk_Ramdisk=$(call Asterisk_Ramdisk,$1,/var/lib/asterisk)

ifdef JMKE_DOING_MAKE_CONFIG
  ifndef ASTERISK_ROOT_MAKE
    ifeq ($(JMKE_PWD_SRC),$(JMK_ROOT)/pkg/voip/asterisk)
      export ASTERISK_ROOT_MAKE=y
    else
      $(error Trying doing make config on asterisk subdir)
    endif
  endif
endif
