include $(TOPDIR)/rules.mk

ARCH:=aarch64
BOARD:=imx8
BOARDNAME:=Freescale i.MX 8
FEATURES:=fpu gpio pcie rtc usb usbgadget squashfs targz nand ubifs boot-part rootfs-part
CPU_TYPE:=cortex-a35
CPU_SUBTYPE:=neon
MAINTAINER:=wangyb <wangyb@genvict.com>

KERNEL_PATCHVER:=4.14

include $(INCLUDE_DIR)/target.mk

KERNELNAME:=Image dtbs

$(eval $(call BuildTarget))
