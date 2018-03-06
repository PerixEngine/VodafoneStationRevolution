ifdef CONFIG_MACH_ADI_FUSIV
  SERIAL=serial_fusiv.o
  LOADADDR=0x81000000
  MKIMG = $(TOOLS_PREFIX)/bin/mkbootimg $(MKIMG_ARGS)
endif
ifdef CONFIG_MACH_IKAN_MIPS
  SERIAL=serial_fusiv.o
  LOADADDR=0x81000000
  MKIMG = $(TOOLS_PREFIX)/bin/mkbootimg $(MKIMG_ARGS)
endif
ifdef CONFIG_BCM963XX
  SERIAL=serial_bcm963xx.o
  LOADADDR=0x82000000
  KERNEL_LDR_OFFSET=16384
  BOOTSTRAP=bootstrap_bcmp63xx.o
  JMK_CFLAGS_serial_bcm963xx.o+=-I$(JMK_ROOT)/vendor/broadcom/bcm963xx/linux-2.6/bcmdrivers/opensource/include/bcm963xx/ -I$(JMK_ROOT)/vendor/broadcom/bcm963xx/linux-2.6/shared/opensource/include/bcm963xx/
endif
ifdef CONFIG_BCM63XX
  SERIAL=serial_bcm63xx.o
  LOADADDR=0x80C00000
  KERNEL_LDR_OFFSET=16384
endif
ifneq ($(CONFIG_DANUBE)$(CONFIG_AR9)$(CONFIG_VR9),)
  SERIAL=serial_danube.o
  LOADADDR=$(if $(CONFIG_VR9),0x84000000,0x80C00000)
  MKIMG = $(JMKE_BUILDDIR)/pkg/boot/u-boot/mkimage $(MKIMG_ARGS)
endif
ifdef CONFIG_CAVIUM_OCTEON
  SERIAL=serial_cavium.o
  LOADADDR=0x0e800000
  # NOTE: we have 1M free space at MAllOC_START_ADDR. After that we start
  # damaging the uboot boot_ptr.
  MALLOC_START=0xc00000
endif
ifneq ($(CONFIG_BROADCOM_9636X)$(CONFIG_BROADCOM_9636X_3X),)
  SERIAL=serial_bcm9636x.o
  ifndef CONFIG_RGLOADER_RESERVED_RAM_START
    LOADADDR=0x82000000
  else
    LOADADDR:=$(shell printf "%\#08x\n" \
      $$(($(CONFIG_RGLOADER_RESERVED_RAM_START)+0x80000000)))
  endif
  include $(JMK_ROOT)/vendor/broadcom/envir.subdirs.mak
  JMK_CFLAGS_serial_bcm9636x.o+=\
    -I$(BCM9636X_ROOT)/bcmdrivers/opensource/include/bcm963xx \
    -I$(BCM9636X_ROOT)/shared/opensource/include/bcm963xx
endif

KERNEL_START=$(call GET_SYMBOL, _text)
KERNEL_ENTRY=$(call GET_SYMBOL, kernel_entry)
KERNEL_END=$(call GET_SYMBOL, _end)

HAVE_MEMCPY=y
