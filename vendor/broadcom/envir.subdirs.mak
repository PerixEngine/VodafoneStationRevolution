# this file is included from several places in the rg tree

ifdef CONFIG_BROADCOM_9636X
  ifdef CONFIG_BROADCOM_BCM9636X_BSP_4_06
    BCM9636X_ROOT=$(JMK_ROOT)/vendor/broadcom/bcm9636x_4.06
    BCM9636X_BUILDDIR=$(JMKE_BUILDDIR)/vendor/broadcom/bcm9636x_4.06
  else
    BCM9636X_ROOT=$(JMK_ROOT)/vendor/broadcom/bcm9636x
    BCM9636X_BUILDDIR=$(JMKE_BUILDDIR)/vendor/broadcom/bcm9636x
  endif
endif  
ifdef CONFIG_BROADCOM_9636X_3X
  BCM9636X_ROOT=$(JMK_ROOT)/vendor/broadcom/bcm9636x-3.x
  BCM9636X_BUILDDIR=$(JMKE_BUILDDIR)/vendor/broadcom/bcm9636x-3.x
endif
