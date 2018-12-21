include	$(TOPDIR)/rules.mk	
PKG_NAME:=czmq_server
  
PKG_RELEASE:=1
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)
include $(INCLUDE_DIR)/package.mk

define Package/czmq_server	
	SECTION:=utils	
	CATEGORY:=Utilities	
	TITLE:=CZMQ SERVER  Program
	DEPENDS:=+libzmq +czmq
endef

define Package/czmq_server/description  
	a sample OpenWRT SERVER program	
endef	

define  Build/Prepare	
	mkdir -p $(PKG_BUILD_DIR)	
	$(CP) ./src/* $(PKG_BUILD_DIR)/	
endef

define Package/czmq_server/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/czmq_server $(1)/usr/bin/
endef

$(eval $(call BuildPackage,czmq_server))	
