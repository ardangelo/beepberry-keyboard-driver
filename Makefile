obj-m += bbqX0kbd.o
bbqX0kbd-objs += src/main.o src/input_iface.o src/params_iface.o src/sysfs_iface.o
ccflags-y := -DDEBUG -g -std=gnu99 -Wno-declaration-after-statement

.PHONY: all clean install uninstall

# LINUX_DIR is set by Buildroot, but not if running manually
ifeq ($(LINUX_DIR),)
LINUX_DIR := /lib/modules/$(shell uname -r)/build
endif

BOOT_CONFIG_LINE := dtoverlay=i2c-bbqX0kbd,irq_pin=4

all: bbqX0kbd.ko i2c-bbqX0kbd.dtbo

bbqX0kbd.ko: src/*.c src/*.h
	$(MAKE) -C '$(LINUX_DIR)' M='$(shell pwd)' modules

i2c-bbqX0kbd.dtbo: i2c-bbqX0kbd.dts
	dtc -@ -I dts -O dtb -W no-unit_address_vs_reg -o $@ $<

install: bbqX0kbd.ko bbqX0kbd.map i2c-bbqX0kbd.dtbo
	# Install kernel module
	$(MAKE) -C '$(LINUX_DIR)' M='$(shell pwd)' modules_install
	# Install keymap
	mkdir -p /usr/local/share/kbd/keymaps/
	install -D -m 0644 bbqX0kbd.map /usr/local/share/kbd/keymaps/
	# Install device tree overlay
	install -D -m 0644 i2c-bbqX0kbd.dtbo /boot/overlays/
	# Add configuration line if it wasn't already there
	grep -qxF '$(BOOT_CONFIG_LINE)' /boot/config.txt \
		|| echo '$(BOOT_CONFIG_LINE)' >> /boot/config.txt
	# Add auto-load module line if it wasn't already there
	grep -qxF 'bbqX0kbd' /etc/modules \
		|| echo 'bbqX0kbd' >> /etc/modules
	# Rebuild dependencies
	depmod -A

uninstall:
	# Remove auto-load module line and create a backup file
	sed -i.save '/bbqX0kbd/d' /etc/modules
	# Remove configuration line and create a backup file
	sed -i.save '/$(BOOT_CONFIG_LINE)/d' /boot/config.txt
	# Remove device tree overlay
	rm -f /boot/overlays/i2c-bbqX0kbd.dtbo
	# Remove keymap
	rm -f /usr/local/share/kbd/keymaps/bbqX0kbd.map

clean:
	$(MAKE) -C '$(LINUX_DIR)' M='$(shell pwd)' clean
