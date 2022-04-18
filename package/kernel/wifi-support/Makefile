#
# Copyright (C) 2008 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=wifi-support
PKG_RELEASE:=3

include $(INCLUDE_DIR)/package.mk

define KernelPackage/wifi-support
  SUBMENU:=I2C support
  TITLE:=Custom wifi-support device
  DEPENDS:=+kmod-cfg80211
  FILES:=$(PKG_BUILD_DIR)/mlan.ko $(PKG_BUILD_DIR)/sd8xxx.ko
#  AUTOLOAD:=$(call AutoLoad,51,mlan sd8xxx,1)
  KCONFIG:=
endef

define KernelPackage/wifi-support/description
 Kernel module for register a custom wifi-support platform device.
endef

EXTRA_KCONFIG:= \
	CONFIG_WIFI-SUPPORT=m

EXTRA_CFLAGS:= \
	$(patsubst CONFIG_%, -DCONFIG_%=1, $(patsubst %=m,%,$(filter %=m,$(EXTRA_KCONFIG)))) \
	$(patsubst CONFIG_%, -DCONFIG_%=1, $(patsubst %=y,%,$(filter %=y,$(EXTRA_KCONFIG)))) \

MAKE_OPTS:= \
	$(KERNEL_MAKE_FLAGS) \
	SUBDIRS="$(PKG_BUILD_DIR)" \
	EXTRA_CFLAGS="$(EXTRA_CFLAGS)" \
	$(EXTRA_KCONFIG)

#define Build/Compile
#	$(MAKE) -C "$(LINUX_DIR)" \
#		$(MAKE_OPTS) \
#		modules
#endef

define Build/Compile
    cd $(LINUX_DIR); \
		ARCH=arm64 CROSS_COMPILE="$(KERNEL_CROSS)" \
        $(MAKE) M=$(PKG_BUILD_DIR) V=1 modules
endef

define KernelPackage/wifi-support/install
	$(INSTALL_DIR) $(1)/lib/firmware/mrvl
	$(INSTALL_BIN) ./files/lib/firmware/mrvl/sd8787_uapsta.bin $(1)/lib/firmware/mrvl
endef

$(eval $(call KernelPackage,wifi-support))
