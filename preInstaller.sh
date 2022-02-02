#!/bin/sh

sudo sh -c "echo dtoverlay=i2c-bbqX0kbd,irq_pin=22 >> /boot/config.txt"
sudo sh -c "echo bbqX0kbd >> /etc/modules"
sudo sh -c "echo KMAP=\"/usr/local/share/kbd/keymaps/bbq10kbd.map\" >> /etc/default/keyboard"
sudo mkdir -p /usr/local/share/kbd/keymaps/
sudo cp source/mod_src/bbq10kbd.map /usr/local/share/kbd/keymaps/bbq10kbd.map

