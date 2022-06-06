#!/bin/bash

if sudo rmmod bbqX0kbd ; then

	sudo sed -i '/bbqX0kbd/d' /boot/config.txt
	sudo rm -rf /boot/overlays/i2c-bbqX0kbd.dtbo
	rm -rf source/dts_src/i2c-bbqX0kbd.dts

	sudo sed -i '/bbqX0kbd/d' /etc/modules
	sudo rm -rf /lib/modules/$(uname -r)/kernel/drivers/i2c/bbqX0kbd.ko

	sudo sed -i '/bbqX0kbd/d' /etc/default/keyboard
	sudo rm -rf /usr/local/share/kbd/keymaps/bbqX0kbd.map

	make clean
else
	echo "BBQX0KBD Driver is not being used.\nIf it was removed manually, remove contents from  files and locations manually.\nRefer to the contents of remover.sh to find what to remove from where.\n\n"
fi
