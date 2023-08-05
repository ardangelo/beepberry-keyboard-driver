obj-m += bbqX0kbd.o
bbqX0kbd-objs += src/main.o src/input_iface.o src/params_iface.o src/sysfs_iface.o
ccflags-y := -DDEBUG -g -std=gnu99 -Wno-declaration-after-statement

.PHONY: all clean

all:
	$(MAKE) -C '$(LINUX_DIR)' M='$(PWD)' modules

clean:
	$(MAKE) -C '$(LINUX_DIR)' M='$(PWD)' clean
