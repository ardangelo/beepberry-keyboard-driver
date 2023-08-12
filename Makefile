obj-m += beepy-kbd.o
beepy-kbd-objs += src/main.o src/input_iface.o src/params_iface.o \
	src/sysfs_iface.o src/ioctl_iface.o
ccflags-y := -DDEBUG -g -std=gnu99 -Wno-declaration-after-statement

.PHONY: all clean install uninstall

# LINUX_DIR is set by Buildroot, but not if running manually
ifeq ($(LINUX_DIR),)
LINUX_DIR := /lib/modules/$(shell uname -r)/build
endif

BOOT_CONFIG_LINE := dtoverlay=beepy-kbd,irq_pin=4

all: beepy-kbd.ko beepy-kbd.dtbo

beepy-kbd.ko: src/*.c src/*.h
	$(MAKE) -C '$(LINUX_DIR)' M='$(shell pwd)' modules

beepy-kbd.dtbo: beepy-kbd.dts
	dtc -@ -I dts -O dtb -W no-unit_address_vs_reg -o $@ $<

install: beepy-kbd.ko beepy-kbd.map beepy-kbd.dtbo
	# Install kernel module
	$(MAKE) -C '$(LINUX_DIR)' M='$(shell pwd)' modules_install
	# Install keymap
	mkdir -p /usr/local/share/kbd/keymaps/
	install -D -m 0644 beepy-kbd.map /usr/local/share/kbd/keymaps/
	# Install device tree overlay
	install -D -m 0644 beepy-kbd.dtbo /boot/overlays/
	# Add configuration line if it wasn't already there
	grep -qxF '$(BOOT_CONFIG_LINE)' /boot/config.txt \
		|| echo '[all]\n$(BOOT_CONFIG_LINE)' >> /boot/config.txt
	# Add auto-load module line if it wasn't already there
	grep -qxF 'beepy-kbd' /etc/modules \
		|| echo 'beepy-kbd' >> /etc/modules
	# Rebuild dependencies
	depmod -A

uninstall:
	# Remove auto-load module line and create a backup file
	sed -i.save '/beepy-kbd/d' /etc/modules
	# Remove configuration line and create a backup file
	sed -i.save '/$(BOOT_CONFIG_LINE)/d' /boot/config.txt
	# Remove device tree overlay
	rm -f /boot/overlays/beepy-kbd.dtbo
	# Remove keymap
	rm -f /usr/local/share/kbd/keymaps/beepy-kbd.map

clean:
	$(MAKE) -C '$(LINUX_DIR)' M='$(shell pwd)' clean
