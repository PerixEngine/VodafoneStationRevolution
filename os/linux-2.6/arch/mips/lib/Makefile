#
# Makefile for MIPS-specific library files..
#

lib-y	+= csum_partial.o delay.o memcpy-inatomic.o memset.o strlen_user.o \
	   strncpy_user.o strnlen_user.o uncached.o

MEMCPY=memcpy.o
ifdef CONFIG_MIPS_PATENTFREE
  MEMCPY = memcpy_NP.o
endif
ifdef CONFIG_CPU_CAVIUM_OCTEON
  MEMCPY = memcpy-octeon.o
endif

lib-y	+= $(MEMCPY)

obj-y			+= iomap.o
obj-$(CONFIG_PCI)	+= iomap-pci.o

# libgcc-style stuff needed in the kernel
lib-y += ashldi3.o ashrdi3.o lshrdi3.o ucmpdi2.o
