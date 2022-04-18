include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/image.mk

#################################################
# Devices
#################################################

KERNEL_LOADADDR=0x80280000
PROJECT_NAME=ubox
#镜像类型：ALL全量包 TAR增量包
IMAGE_TYPE=ALL

define Device/$(PROJECT_NAME)
  DEVICE_VENDOR := WTHINK
  DEVICE_MODEL := UBOX
  KERNEL_NAME := Image
  BOARD_NAME := ubox
  KERNEL_SUFFIX := -kernel.bin
  FDT_LOADADDR = 0x83000000
  DEVICE_DTS := freescale/fsl-imx8qxp-mek
  KERNEL := kernel-bin | lzma | fit lzma $$(DTS_DIR)/$$(DEVICE_DTS).dtb
  FILESYSTEMS := squashfs
  DEVICE_PACKAGES := uboot-envtools
  SUPPORTED_DEVICES := $(subst _,$(comma),$(1))
  IMAGES = root system sysupgrade.bin
  IMAGE/root = append-rootfs
  IMAGE/system = append-kernel | pad-to 4k | append-rootfs | pad-to 4k
  IMAGE/sysupgrade.bin = sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += $(PROJECT_NAME)

define Image/Build

endef

$(eval $(call BuildImage))
