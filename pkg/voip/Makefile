ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../..
  include $(JMK_ROOT)/jmk/env_root.mak
endif

ifdef CONFIG_RG_VOIP

ifdef CONFIG_HW_DSP
  JMK_SUBDIRS+=dsp
endif

JMK_SUBDIRS+=common main

JMK_A_TARGET=libvoip.a
JMK_L_OBJS_$(JMK_A_TARGET)+=main/voip_main.o

ifneq ($(CONFIG_RG_WBM),)
  JMK_SUBDIRS+=wbm
  JMK_JPKG_EXPORTED_DIR+=wbm
  JMK_L_OBJS_$(JMK_A_TARGET)+=wbm/wbm.o
endif

ifneq ($(CONFIG_RG_PHONEBOOK_DB)$(CONFIG_RG_ADDRESS_BOOK),)
  JMK_SUBDIRS+=phonebook
  JMK_O_OBJS_$(JMK_A_TARGET)+=phonebook/phonebook.o
endif

endif

ifdef CONFIG_RG_TR_104
  JMK_SUBDIRS+=tr_104
endif

ifdef CONFIG_RG_VOIP_RV
  JMK_SUBDIRS+=jata_rv
endif

ifdef CONFIG_RG_VOIP_OSIP
  JMK_SUBDIRS+=osip exosip jata_osip
  JMK_L_OBJS_$(JMK_A_TARGET)+=jata_osip/jata_osip.o
endif

ifdef CONFIG_RG_VOIP_HW_EMULATION
  JMK_SUBDIRS+=hwemu
endif

ifdef CONFIG_IXJ
  JMK_SUBDIRS+=ixj
endif

ifdef CONFIG_ZAPTEL
  JMK_SUBDIRS+=zaptel
endif

ifdef CONFIG_RG_VOIP_ASTERISK
  ifdef CONFIG_RG_VOIP_ASTERISK_H323
    JMK_SUBDIRS+=pwlib openh323
    JMK_JPKG_EXPORTED_DIR_SRC+=pwlib openh323
    JMK_LINK_DIRS+=pwlib openh323
  endif
  JMK_SUBDIRS+=asterisk jasterisk
  JMK_L_OBJS_$(JMK_A_TARGET)+=jasterisk/target.o
  JMK_JPKG_EXPORTED_DIR_SRC+=asterisk
  JMK_LINK_DIRS+=asterisk
  ifdef CONFIG_RG_PBX
    JMK_SUBDIRS+=sox
    JMK_JPKG_EXPORTED_DIR_SRC+=sox
    JMK_LINK_DIRS+=sox
    ifdef CONFIG_RG_VOIP_VOICEMAIL
      ifdef CONFIG_RG_SQLITE
      JMK_SO_TARGET=libvoip_dbm.so
      JMK_O_OBJS_$(JMK_SO_TARGET)+=voicemail_dbm.o voicemail_common_utils.o
      ifdef CONFIG_RG_PHONEBOOK_DB
        JMK_O_OBJS_$(JMK_SO_TARGET)+=phonebook/pb_dbm.o phonebook/pb_db_utils.o
      endif
      JMK_RAMDISK_LIB_FILES+=$(JMK_SO_TARGET)
      JMK_EXPORT_HEADERS+=voicemail_dbm.h voicemail_common_utils.h
      JMK_EXPORT_HEADERS_DIR=voip/dbm
      JMK_LIBS_$(JMK_A_TARGET)+=libvoip_dbm.so

      ifndef CONFIG_RG_GPL
      JMK_LOCAL_TARGET+=local_voicemail_dbm_test
      JMK_JPKG_TARGET_BIN_local_voicemail_dbm_test=y
      JMK_O_OBJS_local_voicemail_dbm_test+=voicemail_dbm_test.o voicemail_dbm.o
      JMK_LIBS_local_voicemail_dbm_test+= \
          $(JMKE_BUILDDIR)/pkg/util/liblocal_jutil.a
      JMK_LIBS_local_voicemail_dbm_test+= \
      	  $(JMKE_BUILDDIR)/pkg/sqlite/liblocal_sqlite3.a
      JMK_JPKG_EXPORTED_FILES+=voicemail_dbm_test.c
      JMK_RUN_UNITTEST+=voicemail_dbm_test
      endif
      endif
    endif
  endif
endif

ifdef CONFIG_RG_JRTP
  JMK_SUBDIRS+=jrtp
endif

ifdef CONFIG_RG_ALARM_CLOCK
  JMK_SUBDIRS+=alarm_clock
  JMK_L_OBJS_$(JMK_A_TARGET)+=alarm_clock/alarm_clock.o
endif

ifdef CONFIG_RG_VOIP_DSP_TRANSCODING
  JMK_SUBDIRS+=jconvert
endif

ifdef CONFIG_RG_GPL
   JMK_RAMDISK_BIN_FILES=
   JMK_A_TARGET=
   JMK_FIRST_TASKS=
   JMK_TARGET:=$(filter josip,$(JMK_TARGET))
   JMK_SUBDIRS:=$(filter %osip callctrl arch asterisk openh323 pwlib dsp jrtp \
       jtimer common phonebook,$(JMK_SUBDIRS))
endif

$(call JMKE_INCLUDE_RULES)
