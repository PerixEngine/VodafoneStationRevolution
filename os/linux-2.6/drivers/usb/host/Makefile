#
# Makefile for USB Host Controller Drivers
#

ifeq ($(CONFIG_USB_DEBUG),y)
	EXTRA_CFLAGS		+= -DDEBUG
endif

ifndef CONFIG_ARCH_M821XX
obj-$(CONFIG_PCI)		+= pci-quirks.o
endif

obj-$(CONFIG_USB_EHCI_HCD)	+= ehci-hcd.o
obj-$(CONFIG_USB_ISP116X_HCD)	+= isp116x-hcd.o
obj-$(CONFIG_USB_OHCI_HCD)	+= ohci-hcd.o
obj-$(CONFIG_USB_UHCI_HCD)	+= uhci-hcd.o
obj-$(CONFIG_USB_SL811_HCD)	+= sl811-hcd.o
obj-$(CONFIG_USB_SL811_CS)	+= sl811_cs.o
obj-$(CONFIG_USB_U132_HCD)	+= u132-hcd.o
obj-$(CONFIG_ETRAX_ARCH_V10)	+= hc_crisv10.o
obj-$(CONFIG_USB_DWC_OTG)	+= dwc_otg/
obj-$(CONFIG_USB_M82XX_HCD)	+= m82xx-hcd.o
obj-$(CONFIG_FSL_USB20)		+= fsl-usb.o

