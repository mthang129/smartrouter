include	$(TOPDIR)/rules.mk	
PKG_NAME:=czmq_client
  
PKG_RELEASE:=1
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)
include $(INCLUDE_DIR)/package.mk

define Package/czmq_client	
	SECTION:=utils	
	CATEGORY:=Utilities	
	TITLE:=CZMQ CLIENT  Program
	DEPENDS:=+libzmq +czmq +libiwinfo
endef

define Package/czmq_client/description  
	a sample OpenWRT czmq_client program	
endef	

define  Build/Prepare	
	mkdir -p $(PKG_BUILD_DIR)	
	$(CP) ./src/* $(PKG_BUILD_DIR)/	
endef

define Package/czmq_client/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/czmq_client $(1)/usr/bin/
endef

$(eval $(call BuildPackage,czmq_client))	
