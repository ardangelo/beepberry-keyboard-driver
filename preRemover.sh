#!/bin/sh

sudo sed -i '$d' /etc/modules
sudo sed -i '$d' /boot/config.txt
sudo rm -rf /usr/local/share/kbd/keymaps/bbq10kbd.map
sudo sed -i '$d' /etc/default/keyboard
