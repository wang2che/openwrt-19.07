include $(TOPDIR)/rules.mk
 
PKG_NAME:=fiveg-support
PKG_VERSION:=1.0
PKG_BUILD_DIR:= $(BUILD_DIR)/$(PKG_NAME)
 
include $(INCLUDE_DIR)/package.mk
 
define Package/fiveg-support
	SECTION:=base
	CATEGORY:=Utilities
	TITLE:=Hello world -prints a hello world message
endef
 
define Package/fiveg-support/description
	If you can't figure out what this program does, you're probably
	brain-dead and need immediate medical attention.
endef
 
define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Package/fiveg-support/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/muxconnmanager $(1)/usr/bin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/cm_default/oldconnmanager $(1)/usr/bin/
	$(INSTALL_BIN) ./files/usr/bin/* $(1)/usr/bin/
endef
 
$(eval $(call BuildPackage,fiveg-support))
