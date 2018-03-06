JMK_ROOT=../../..
include $(JMK_ROOT)/envir.mak

JMK_SUBDIRS+=linux-2.6 userspace/public/include userspace/private/include

ifdef CONFIG_BCM_FAP
  JMK_SUBDIRS+=userspace/private/libs/fapctl userspace/private/apps/fapctl
endif

# debug tools
#ifdef CONFIG_BCM_INGQOS
#  JMK_SUBDIRS+=userspace/private/libs/iqctl userspace/private/apps/iqctl
#endif
#
#ifdef CONFIG_BCM_BPM
#  JMK_SUBDIRS+=userspace/private/apps/bpmctl
#endif

ifdef CONFIG_BCM_PWRMNGT
  JMK_SUBDIRS+=userspace/private/libs/pwrctl userspace/private/apps/pwrctl
endif

JMK_SUBDIRS+=shared bcmdrivers xChange cfe hostTools \
  $(if $(CONFIG_BCM_XDSLCTL_TOOL),userspace/dsl_tools)
JMK_LINK_DIRS+=shared bcmdrivers xChange linux-2.6 cfe hostTools
JMK_JPKG_EXPORTED_DIR_SRC+=shared bcmdrivers linux-2.6 \
   unihan/userspace/mcuctl/api
JMK_DONT_EXPORT+=linux-2.6/arch/mips/bcm963xx/ktools/*

JMK_JPKG_EXPORTED_FILES+=flash_layout_ywz00a.c flash_layout_ywz00b.c \
  flash_layout_ywz00b_it.c flash_layout_wvdb113g.c flash_layout_bcm96362.c \
  version.make make.voice flash_layout_ywz00b_nz.c flash_layout_vox25_it.c \
  flash_layout_vox25_de.c

JMK_EXPORT_HEADERS_DIR=vendor
JMK_EXPORT_HEADERS+=rgloader.h

ifdef CONFIG_BCM9636X_SUPPORT_SWMDK
  MDK212_PATH=userspace/private/libs/mdk212
  JMK_SUBDIRS+=userspace/private/apps/swmdk userspace/private/libs/ethswctl \
    $(MDK212_PATH)/bmd $(MDK212_PATH)/cdk $(MDK212_PATH)/phy
endif

ifdef CONFIG_RG_FLASH_LAYOUT_BCM963168AC5_REF
  JMK_SUBDIRS+=bcm963168ac5
endif

ifdef CONFIG_ADB_MCU_LEDS
  JMK_SUBDIRS+=unihan/userspace/mcuctl/api userspace/public/include \
    userspace/public/libs/cms_boardctl/linux
endif

ifdef CONFIG_RG_FLASH_LAYOUT_BCM96362_REF
  JMK_EXPORT_HEADERS+=flash_layout_bcm96362.c__c/flash_layout.c
endif
ifdef CONFIG_RG_FLASH_LAYOUT_VF_DE_2_5
  JMK_EXPORT_HEADERS+=flash_layout_vox25_de.c__c/flash_layout.c
endif
ifdef CONFIG_RG_FLASH_LAYOUT_VF_IT_2_5
  JMK_EXPORT_HEADERS+=flash_layout_vox25_it.c__c/flash_layout.c
endif
ifdef CONFIG_RG_FLASH_LAYOUT_VF_UK_2_5
  JMK_EXPORT_HEADERS+=flash_layout_vox25_it.c__c/flash_layout.c
endif
ifdef CONFIG_RG_FLASH_LAYOUT_YWZ00A_REF
  JMK_EXPORT_HEADERS+=flash_layout_ywz00a.c__c/flash_layout.c
endif
ifdef CONFIG_RG_FLASH_LAYOUT_VF_IT_1_5
  JMK_EXPORT_HEADERS+=flash_layout_ywz00b_it.c__c/flash_layout.c
endif
ifdef CONFIG_RG_FLASH_LAYOUT_VF_NZ_1_5
  JMK_EXPORT_HEADERS+=flash_layout_ywz00b_nz.c__c/flash_layout.c
endif
ifdef CONFIG_RG_FLASH_LAYOUT_VF_ES_1_5
  JMK_EXPORT_HEADERS+=flash_layout_ywz00b.c__c/flash_layout.c
endif
ifdef CONFIG_RG_FLASH_LAYOUT_VF_PT_1_5
  JMK_EXPORT_HEADERS+=flash_layout_ywz00b.c__c/flash_layout.c
endif
ifdef CONFIG_RG_FLASH_LAYOUT_YWZ00B_REF
  JMK_EXPORT_HEADERS+=flash_layout_ywz00b.c__c/flash_layout.c
endif
ifdef CONFIG_RG_FLASH_LAYOUT_HG558BZA_REF
  JMK_EXPORT_HEADERS+=flash_layout_ywz00b.c__c/flash_layout.c
endif
ifdef CONFIG_RG_FLASH_LAYOUT_WVDB113G_REF
  JMK_EXPORT_HEADERS+=flash_layout_wvdb113g.c__c/flash_layout.c
endif

JMK_ARCHCONFIG_LAST_TASKS+=link_linux_includes

BC_KERNEL_INCLUDE=$(JMKE_PWD_BUILD)/kernel/linux/include

link_linux_includes: 
	$(JMKE_MKDIR) $(BC_KERNEL_INCLUDE)
	$(JMKE_CP_LN) $(JMK_ROOT)/os/linux-2.6/include/. $(BC_KERNEL_INCLUDE)
	$(JMKE_CP) $(JMKE_BUILDDIR)/os/linux-2.6/include/. $(BC_KERNEL_INCLUDE)

include $(JMKE_MK)
