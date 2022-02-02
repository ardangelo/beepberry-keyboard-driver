# bbq10pmod Kernel Module

## Raspberry Pi

### Wiring

```
Pi          -   Module
3V3         -   3V3
GND         -   GND
GPIO2/SDA1  -   SDA
GPIO3/SCL1  -   SCL
GPIO11      -   INT
```

### Installation

Installing this Kernel Module on a Raspberry Pi is quite straight forward.

* `sudo apt-get update && sudo apt-get upgrade -y && sudo apt-get install git raspberrypi-kernel-headers`
* `git clone https://github.com/arturo182/bbq10pmod_module.git && cd bbq10pmod_module`
* `make && make dtbo`
* `sudo cp bbq10pmod.ko /lib/modules/4.19.57+/kernel/drivers/i2c/bbq10pmod.ko`
* `sudo cp i2c-bbq10pmod.dtbo /boot/overlays/`
* `sudo sh -c "echo dtoverlay=i2c-bbq10pmod >> /boot/config.txt"`
* `sudo sh -c "echo bbq10pmod >> /etc/modules"`
* `sudo depmod`
* `sudo reboot`

After these steps you should have the PMOD working on your Raspberry Pi. You can check this via `dmesg`:
```
[    8.249265] i2c /dev entries driver
[    8.337603] bbq10pmod: loading out-of-tree module taints kernel.
[...]
[   16.524469] bbq10pmod_probe version: 0x02
[   16.536031] input: bbq10pmod as /devices/platform/soc/20804000.i2c/i2c-1/1-001f/input/input0
```
