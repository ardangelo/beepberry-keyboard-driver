# SPDX-License-Identifier: GPL-2.0
# Keyboard Driver for Blackberry Keyboards BBQ10 from arturo182. Software written by wallComputer.

include Kbuild
CFLAGS_$(MODULE_NAME)_main.o := -DDEBUG

modules:
	make -C $(KDIR) M=$(PWD) modules

dtbo:
	dtc -I dts -O dtb -o i2c-bbqX0kbd.dtbo source/dts_src/i2c-bbqX0kbd.dts

clean:
	make -C $(KDIR) M=$(PWD) clean
	rm -rf i2c-bbqX0kbd.dtbo


