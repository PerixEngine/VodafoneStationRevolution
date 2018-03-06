#
# Makefile for the Linux/MIPS-specific parts of the memory manager.
#

ifndef CONFIG_CPU_CAVIUM_OCTEON
# The Octeon uses its own version of the DMA mapping routines
# to allow 32bit devices to access memory higher than 4GB. The
# Octeon version of this file is arch/mips/cavium-octeon/dma-octeon.c
# tlbex-octeon.c contains Octeon-specific modifications for tlbex.c
obj-y				+= dma-default.o tlbex.o
endif

obj-y				+= cache.o extable.o fault.o \
				   init.o pgtable.o tlbex-fault.o

obj-$(CONFIG_32BIT)		+= ioremap.o pgtable-32.o
obj-$(CONFIG_64BIT)		+= pgtable-64.o
obj-$(CONFIG_HIGHMEM)		+= highmem.o
obj-$(CONFIG_HUGETLB_PAGE)	+= hugetlbpage.o

obj-$(CONFIG_CPU_MIPS32)	+= c-r4k.o cex-gen.o pg-r4k.o tlb-r4k.o
obj-$(CONFIG_CPU_MIPS64)	+= c-r4k.o cex-gen.o pg-r4k.o tlb-r4k.o
obj-$(CONFIG_CPU_NEVADA)	+= c-r4k.o cex-gen.o pg-r4k.o tlb-r4k.o
obj-$(CONFIG_CPU_R10000)	+= c-r4k.o cex-gen.o pg-r4k.o tlb-r4k.o
obj-$(CONFIG_CPU_R3000)		+= c-r3k.o tlb-r3k.o pg-r4k.o
obj-$(CONFIG_CPU_LX4189)	+= c-lx4189.o tlb-r3k.o pg-r4k.o
obj-$(CONFIG_CPU_R4300)		+= c-r4k.o cex-gen.o pg-r4k.o tlb-r4k.o
obj-$(CONFIG_CPU_R4X00)		+= c-r4k.o cex-gen.o pg-r4k.o tlb-r4k.o
obj-$(CONFIG_CPU_R5000)		+= c-r4k.o cex-gen.o pg-r4k.o tlb-r4k.o
obj-$(CONFIG_CPU_R5432)		+= c-r4k.o cex-gen.o pg-r4k.o tlb-r4k.o
obj-$(CONFIG_CPU_R8000)		+= c-r4k.o cex-gen.o pg-r4k.o tlb-r8k.o
obj-$(CONFIG_CPU_RM7000)	+= c-r4k.o cex-gen.o pg-r4k.o tlb-r4k.o
obj-$(CONFIG_CPU_RM9000)	+= c-r4k.o cex-gen.o pg-r4k.o tlb-r4k.o
obj-$(CONFIG_CPU_SB1)		+= c-sb1.o cerr-sb1.o cex-sb1.o pg-sb1.o \
				   tlb-r4k.o
obj-$(CONFIG_CPU_TX39XX)	+= c-tx39.o pg-r4k.o tlb-r3k.o
obj-$(CONFIG_CPU_TX49XX)	+= c-r4k.o cex-gen.o pg-r4k.o tlb-r4k.o
obj-$(CONFIG_CPU_VR41XX)	+= c-r4k.o cex-gen.o pg-r4k.o tlb-r4k.o
obj-$(CONFIG_CPU_CAVIUM_OCTEON)	+= c-octeon.o cex-oct.o pg-octeon.o tlb-r4k.o \
				   tlbex-octeon.o

obj-$(CONFIG_IP22_CPU_SCACHE)	+= sc-ip22.o
obj-$(CONFIG_R5000_CPU_SCACHE)  += sc-r5k.o
obj-$(CONFIG_RM7000_CPU_SCACHE)	+= sc-rm7k.o
obj-$(CONFIG_MIPS_CPU_SCACHE)	+= sc-mips.o
