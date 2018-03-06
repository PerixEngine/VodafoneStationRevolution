JMK_ROOT=../../../../../
include $(JMK_ROOT)/envir.mak

include envir.mk

JMK_MOD_TARGET += rg_prom_mod.o
JMK_O_OBJS_rg_prom_mod.o = rg_prom.o
JMK_MOD_2_STAT = $(JMK_MOD_TARGET)

JMK_CFLAGS=$(JMK_RG_LINUX_CFLAGS) -I$(RG_INCLUDE_RG_LINUX_DIR) -DHEAD_ADDR=$(LOADADDR) -DKERNEL_END=$(KERNEL_END)

OBJS_FOR_JPKG=$(if $(CONFIG_BCM963XX_BOOTSTRAP),$(BOOTSTRAP)) head.o $(SERIAL) \
  handoff.o

ifneq ($(CONFIG_CPU_MIPS32)$(CONFIG_CPU_MIPS64),)
  OBJS_FOR_JPKG+=c-r4k.o
endif

OBJS+=$(OBJS_FOR_JPKG)
JMK_TARGET_CC_OBJS=$(OBJS)

JMK_CD_EXPORTED_FILES+=$(SERIAL:%.o=%.c) ld.script

JMK_CLEAN+=$(OBJS) boot.o
JMK_CLEAN+=dummy jmp_boot.o jmp_boot.img jmp_boot.elf jmp_boot.bin jmp_boot.lz

include $(JMKE_MK)

ifdef JMKE_IS_BUILDDIR
boot.o: $(OBJS)
	$(LD) -r $(OBJS) -o $@

# TODO CONFIG_JMPBOOT or something
ifneq ($(CONFIG_BCM963XX)$(CONFIG_BCM63XX),)
ifdef CONFIG_BOOTLDR_CFE
JMP_BOOT_ADDR=0x80010000
OPENRG_SECTION_OFFSET=$(shell $(JMKE_BUILDDIR)/pkg/perm_storage/image_flash_offset)
# The address of the kernel.o on the flash
KERNEL_ADDR=$$(( $(CONFIG_MTD_PHYSMAP_START)|0xb0000000 + $(OPENRG_SECTION_OFFSET) + $(KERNEL_LDR_OFFSET)))

BCM_DIR=$(JMK_ROOT)/vendor/broadcom/bcm963xx/linux-2.6/

ifdef CONFIG_BCM963XX
  BCM_CHIP=$(CONFIG_BCM963XX_CHIP)
  BCM_BOARD_ID=$(CONFIG_BCM963XX_BOARD_ID)
  BCM_BLKSIZE=$(if $(filter DWV_BCM96358 CT6382D,$(CONFIG_RG_HW)),64,128)
endif
ifdef CONFIG_BCM63XX
  # XXX although we are in kernel-3.x, we take advantage of hostTools from 2.6.
  # This is a shortcut which is acceptable because we don't plan to do formal
  # deliveries for this in-tree platform.
  # We can only set BCM_CHIP for sure - 6358. BCM_BOARD_ID and BCM_BLKSIZE are
  # really hw-specific, but since our hw is "generic kernel in-tree BCM6358",
  # we set values fitting the boards we run kernel-3.x on - e.g. Dlink-Telus.
  BCM_CHIP=6358
  BCM_BOARD_ID=96358VW
  BCM_BLKSIZE=64
endif

ifneq (,$(filter $(CONFIG_RG_HW),HG55MAGV HG55VDFA))
BCM_IMAGE_BUILDER_TOOL=bcmImageBuilderHG553
else
BCM_IMAGE_BUILDER_TOOL=bcmImageBuilder
endif

jmp_boot.img: jmp_boot.elf
	$(OBJCOPY) -O binary -R .note -R .comment -R __ex_table -S $< \
		jmp_boot.bin
	LD_LIBRARY_PATH=$(LD_LIBRARY_PATH):/lib:/usr/lib:/lib/i386-linux-gnu:$(I386_TOOLS_PREFIX)/$(I386_TARGET_MACHINE)/lib \
		$(BCM_DIR)/hostTools/cmplzma -k -2 \
		jmp_boot.elf jmp_boot.bin jmp_boot.lz
	touch dummy
	$(BCM_DIR)/hostTools/$(BCM_IMAGE_BUILDER_TOOL) \
		--output $@ \
		--chip $(BCM_CHIP) \
		--board $(BCM_BOARD_ID) \
		--blocksize $(BCM_BLKSIZE) \
		--cfefile $(JMK_ROOT)/vendor/broadcom/bcm963xx/linux-2.6/cfe/cfe$(BCM_CHIP).bin \
		--rootfsfile dummy --kernelfile jmp_boot.lz

jmp_boot.elf: ld_jmp_boot.script jmp_boot.o 
	$(LD) jmp_boot.o -T $< -Ttext $(JMP_BOOT_ADDR) -o $@ --defsym \
		openrg_start=$(KERNEL_ADDR)
endif
endif

endif
