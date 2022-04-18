#
# Copyright (C) 2006-2014 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=uboot-imx8
PKG_DISTNAME:=u-boot-imx8
PKG_VERSION:=2018.03
PKG_RELEASE:=3.1

PKG_SOURCE_PROTO:=git
PKG_SOURCE:=$(PKG_DISTNAME)-$(PKG_VERSION).tar.xz
PKG_SOURCE_SUBDIR:=$(PKG_DISTNAME)-$(PKG_VERSION)
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_DISTNAME)-$(PKG_VERSION)
PKG_MIRROR_HASH:=f6ec08e605aa79e263d5738781ff241e617cd727537060c58723c91421afe574

PKG_BUILD_DEPENDS:=fstools

PKG_LICENSE:=GPL-2.0 GPL-2.0+
PKG_LICENSE_FILES:=Licenses/README

PKG_FLAGS:=nonshared

PKG_BUILD_PARALLEL:=1

include $(INCLUDE_DIR)/package.mk

define Package/uboot-imx8
  SECTION:=utils
  CATEGORY:=Boot Loaders
  TITLE:=imx8 U-Boot bootloader
  URL:=git://source.codeaurora.org/external/imx/uboot-imx.git
endef

define Package/uboot-imx8/description
 This package includes IMX8QXP U-Boot bootloader.
endef

define Build/Configure
	$(CP) -rf ./files/imx-mkimage $(PKG_BUILD_DIR)/
	$(MAKE) -C $(PKG_BUILD_DIR) imx8qxp_mek_defconfig
endef

define Build/Compile
	$(call Build/Compile/Default,ARCH=arm)
	$(CP) $(PKG_BUILD_DIR)/u-boot.bin $(PKG_BUILD_DIR)/imx-mkimage/iMX8QX
	$(MAKE) -C $(PKG_BUILD_DIR)/imx-mkimage SOC=iMX8QX flash
endef

define Package/uboot-imx8/install
	$(INSTALL_DIR) $(STAGING_DIR_IMAGE)
	$(CP) $(PKG_BUILD_DIR)/imx-mkimage/iMX8QX/flash.bin $(BIN_DIR)/$(IMG_PREFIX)
endef

$(eval $(call BuildPackage,uboot-imx8))
