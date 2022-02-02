#!/bin/sh

sudo cp bbqX0kbd.ko /lib/modules/$(uname -r)/kernel/drivers/i2c/bbqX0kbd.ko
sudo cp i2c-bbqX0kbd.dtbo /boot/overlays/
sudo depmod



