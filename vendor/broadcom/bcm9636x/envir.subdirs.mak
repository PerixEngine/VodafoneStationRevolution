BCM_DIR=$(JMKE_BUILDDIR)/vendor/broadcom/bcm9636x
SHARED_DIR=$(BCM_DIR)/shared
BRCMDRIVERS_DIR=$(BCM_DIR)/bcmdrivers
HOSTTOOLS_DIR=$(JMK_ROOT)/vendor/broadcom/bcm9636x/hostTools
HOSTTOOLS_BUILD_DIR=$(BCM_DIR)/hostTools
BCM963XX_BOARDPARMS=$(SHARED_DIR)/opensource/boardparms/bcm963xx

export INC_BRCMDRIVER_PUB_PATH=$(BRCMDRIVERS_DIR)/opensource/include
export INC_BRCMDRIVER_PRIV_PATH=$(BRCMDRIVERS_DIR)/broadcom/include
export INC_ADSLDRV_PATH=$(BRCMDRIVERS_DIR)/broadcom/char/adsl/impl1
export INC_BRCMSHARED_PUB_PATH=$(SHARED_DIR)/opensource/include
export INC_BRCMSHARED_PRIV_PATH=$(SHARED_DIR)/broadcom/include
export INC_BRCMBOARDPARMS_PATH=$(SHARED_DIR)/opensource/boardparms
export INC_SPI_PATH=$(SHARED_DIR)/opensource/spi
export INC_FLASH_PATH=$(SHARED_DIR)/opensource/flash

ifdef CONFIG_SMP
  export BRCM_SMP_BUILD=y
endif

ifdef CONFIG_BCM_BRCM_VOICE_NONDIST
  export BRCM_VOICE_NONDIST=y
endif

ifdef CONFIG_RG_KGDB
  export KERNEL_DEBUG=1
else
  export KERNEL_DEBUG=0
endif

ifdef CONFIG_KALLSYMS
  export KERNEL_KALLSYMS=1
else
  export KERNEL_KALLSYMS=0
endif

export INC_XTMRT_PATH=\
  $(BRCMDRIVERS_DIR)/opensource/net/xtmrt/impl$(CONFIG_BCM_XTMRT_IMPL)

ifdef CONFIG_HW_YWZ00B
  JMK_MOD_CFLAGS+=-include $(SHARED_DIR)/opensource/include/bcm963xx/davinci_def.h
endif

include $(JMK_ROOT)/vendor/broadcom/bcm9636x/version.make

ifdef CONFIG_BCM_ENDPOINT
ifndef CONFIG_RG_JPKG_SRC
  include $(JMK_ROOT)/vendor/broadcom/bcm9636x/make.voice
endif
endif

export XCHG_C_COMPILE := $(TARGET_CC)
export XCHG_AR := $(AR)
export XCHG_ASSEMBLE := $(AS)
export XCHG_C_OPENRG_INCLUDE := $(JMKE_BUILDDIR)/os/linux/include

