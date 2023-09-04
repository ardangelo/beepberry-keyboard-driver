obj-m += beepy-kbd.o
beepy-kbd-objs += src/main.o src/input_iface.o src/params_iface.o \
	src/sysfs_iface.o
ccflags-y := -DDEBUG -g -std=gnu99 -Wno-declaration-after-statement

dtb-y += beepy-kbd.dtbo

targets += $(dtbo-y)    
always  := $(dtbo-y)

.PHONY: all clean install install_modules install_aux uninstall

# LINUX_DIR is set by Buildroot, but not if running manually
ifeq ($(LINUX_DIR),)
LINUX_DIR := /lib/modules/$(shell uname -r)/build
endif

# BUILD_DIR is set by DKMS, but not if running manually
ifeq ($(BUILD_DIR),)
BUILD_DIR := .
endif

BOOT_CONFIG_LINE := dtoverlay=beepy-kbd,irq_pin=4
KMAP_LINE := KMAP=/usr/share/kbd/keymaps/beepy-kbd.map

all:
	$(MAKE) -C '$(LINUX_DIR)' M='$(shell pwd)'

install_modules:
	$(MAKE) -C '$(LINUX_DIR)' M='$(shell pwd)' modules_install
	# Rebuild dependencies
	depmod -A

install: install_modules install_aux

# Separate rule to be called from DKMS
install_aux:
	# Install keymap
	mkdir -p /usr/share/kbd/keymaps/
	install -D -m 0644 $(BUILD_DIR)/beepy-kbd.map /usr/share/kbd/keymaps/
	# Install device tree overlay
	install -D -m 0644 $(BUILD_DIR)/beepy-kbd.dtbo /boot/overlays/
	# Add configuration line if it wasn't already there
	grep -qxF '$(BOOT_CONFIG_LINE)' /boot/config.txt \
		|| printf '[all]\ndtparam=i2c_arm=on\n$(BOOT_CONFIG_LINE)\n' >> /boot/config.txt
	# Add auto-load module line if it wasn't already there
	grep -qxF 'beepy-kbd' /etc/modules \
		|| echo 'beepy-kbd' >> /etc/modules
	# Configure keymap as default
	grep -qxF '$(KMAP_LINE)' /etc/default/keyboard \
		|| echo '$(KMAP_LINE)' >> /etc/default/keyboard
	rm -f /etc/console-setup/cached_setup_keyboard.sh
	dpkg-reconfigure keyboard-configuration \
		|| echo "dpkg-reconfigure failed, keymap may not be applied"

uninstall:
	# Remove auto-load module line and create a backup file
	sed -i.save '/beepy-kbd/d' /etc/modules
	# Remove configuration line and create a backup file
	sed -i.save '/$(BOOT_CONFIG_LINE)/d' /boot/config.txt
	# Remove device tree overlay
	rm -f /boot/overlays/beepy-kbd.dtbo
	# Remove keymap
	rm -f /usr/share/kbd/keymaps/beepy-kbd.map
	# Remove keymap setting
	sed -i.save '/$(KMAP_LINE)/d' /etc/default/keyboard
	rm -f /etc/console-setup/cached_setup_keyboard.sh
	dpkg-reconfigure keyboard-configuration \
		|| echo "dpkg-reconfigure failed, old keymap may not be applied"

clean:
	$(MAKE) -C '$(LINUX_DIR)' M='$(shell pwd)' clean
