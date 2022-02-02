#!/bin/sh

sudo rmmod bbqX0kbd
sudo rm -rf /boot/overlays/i2c-bbqX0kbd.dtbo
sudo rm -rf /lib/modules/$(uname -r)/kernel/drivers/i2c/bbqX0kbd.ko
make clean

