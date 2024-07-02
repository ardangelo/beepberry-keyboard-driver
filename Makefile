obj-m += emate-kbd.o
emate-kbd-objs += src/main.o src/params_iface.o src/sysfs_iface.o \
	src/input_iface.o src/input_fw.o
ccflags-y := -g -std=gnu99 -Wno-declaration-after-statement

.PHONY: all clean install install_modules install_aux uninstall

# KERNELRELEASE is set by DKMS, can be different inside chroot
ifeq ($(KERNELRELEASE),)
KERNELRELEASE := $(shell uname -r)
endif
# LINUX_DIR is set by Buildroot
ifeq ($(LINUX_DIR),)
LINUX_DIR := /lib/modules/$(KERNELRELEASE)/build
endif

# BUILD_DIR is set by DKMS, but not if running manually
ifeq ($(BUILD_DIR),)
BUILD_DIR := .
endif

BOOT_CONFIG_LINE := dtoverlay=emate-kbd

# Raspbian 12 moved config and cmdline to firmware
ifeq ($(wildcard /boot/firmware/config.txt),)
	CONFIG=/boot/config.txt
else
	CONFIG=/boot/firmware/config.txt
endif

all: emate-kbd.dtbo
	$(MAKE) -C '$(LINUX_DIR)' M='$(shell pwd)'

emate-kbd.dtbo: emate-kbd.dts
	dtc -@ -I dts -O dtb -W no-unit_address_vs_reg -o $@ $<

install_modules:
	$(MAKE) -C '$(LINUX_DIR)' M='$(shell pwd)' modules_install
	# Rebuild dependencies
	depmod -A

install: install_modules install_aux

# Separate rule to be called from DKMS
install_aux: emate-kbd.dtbo
	# Install device tree overlay
	install -D -m 0644 $(BUILD_DIR)/emate-kbd.dtbo /boot/overlays/
	# Add configuration line if it wasn't already there
	@grep -qxF '$(BOOT_CONFIG_LINE)' $(CONFIG) \
		|| printf '[all]\ndtparam=i2c_arm=on\n$(BOOT_CONFIG_LINE)\n' >> $(CONFIG)
	# Add auto-load module line if it wasn't already there
	@grep -qxF 'emate-kbd' /etc/modules \
		|| printf 'i2c-dev\nemate-kbd\n' >> /etc/modules

uninstall:
	# Remove auto-load module line and create a backup file
	@sed -i.save '/emate-kbd/d' /etc/modules
	# Remove configuration line and create a backup file
	@sed -i.save '/$(BOOT_CONFIG_LINE)/d' $(CONFIG)
	# Remove device tree overlay
	rm -f /boot/overlays/emate-kbd.dtbo

clean:
	$(MAKE) -C '$(LINUX_DIR)' M='$(shell pwd)' clean
