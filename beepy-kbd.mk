BEEPY_KBD_VERSION = 1.0
BEEPY_KBD_SITE = $(BR2_EXTERNAL_BEEPY_DRIVERS_PATH)/package/beepy-kbd
BEEPY_KBD_SITE_METHOD = local

BEEPY_KBD_INSTALL_IMAGES = YES
BEEPY_KBD_MODULE_SUBDIRS = .

define BEEPY_KBD_BUILD_CMDS
	for dts in $(@D)/*.dts; do \
		$(HOST_DIR)/bin/dtc -@ -I dts -O dtb -W no-unit_address_vs_reg -o $${dts%.dts}.dtbo $${dts}; \
	done
endef

define BEEPY_KBD_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/beepy-kbd.map $(TARGET_DIR)/usr/share/keymaps/;
	$(INSTALL) -D -m 0755 $(@D)/init/S01beepykbd $(TARGET_DIR)/etc/init.d/;
endef

define BEEPY_KBD_INSTALL_IMAGES_CMDS
	for dtbo in $(@D)/*.dtbo; do \
		$(INSTALL) -D -m 0644 $${dtbo} $(BINARIES_DIR)/rpi-firmware/overlays; \
	done
endef

$(eval $(kernel-module))
$(eval $(generic-package))
