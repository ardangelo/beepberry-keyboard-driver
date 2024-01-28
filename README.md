# Beepy keyboard driver

## Alt and Sym modifiers

The alternate symbols printed directly on the Beepy keys are triggered by pressing the physical Alt key, then the key on which the symbol is printed.

For additional symbols not printed directly on the keys, the Sym key is used. Sym sends AltGr (Right Alt), which is mapped to more symbols via the keymap file at `/usr/share/kbd/keymaps/beepy-kbd.map`.

## Sticky modifier keys

The keyboard driver supports sticky modifier keys. Holding a modifier key (Shift, Alt, Sym) while typing an alpha keys will apply the modifier to all alpha keys until the modifier is released.

One press and release of the modifier will enter sticky mode, applying the modifier to the next alpha key only. If the same modifier key is pressed and released again in sticky mode, it will be canceled.

Visual mode indicators are drawn in the top right corner of the display, with indicators for Shift, Physical Alt, Control, Alt, Symbol, and Meta mode.

## Other key mappings

- Physical Alt + Enter is mapped to Tab
- Holding Shift temporarily enables the touch sensor until Shift is released

- Single click
	- Call: Control
	- Berry: Enter Meta mode (see the section on Meta Mode)
	- Touchpad: Enable optical touch sensor, sending arrow keys. Subsequent clicks send Enter
	- Back: Escape
	- End Call: Tmux prefix (customize the prefix in the keymap file)

- Short hold (1 second)
	- Call: Lock Control
	- Berry: Display Meta mode overlay (requires `beepy-symbol-overlay` package)
	- End Call: Open Tmux menu (requires `beepy-tmux-menus` package and Tmux configuration)
	- Symbol: Display Symbol overlay (requires `beepy-symbol-overlay` package)

- Long hold (5 seconds)
	- End call: send shutdown signal to Pi

## Meta mode

Meta mode is a modal layer that assists in rapidly moving the cursor and scrolling with single keypresses. To enter Meta mode, click the touchpad button once. Then, the following keymap is applied, staying in Meta mode until dismissed:

- E: up, S: down, W: left, D: right
    - Why not WASD? This way, you can place your thumb in the middle of all four of these keys, and more fluidly move the cursor without mistyping
- R: Home, F: End, O: PageUp, P: PageDown
- Q: Alt+Left (back one word), A: Alt+Right (forward one word)
- T: Tab (dismisses meta mode)
- X: Apply Control to next key (dismisses meta mode)
- C: Apply Alt to next key (dismisses meta mode)
- 0: Toggle Sharp display inversion
- N: Decrease keyboard brightness
- M: Increase keyboard brightness
- $: Toggle keyboard backlight
- Esc: (Back button): exit meta mode

Typing any other key while in Meta mode will exit Meta mode and send the key as if it was typed normally.

## `sysfs` Interface

The following sysfs entries are available under `/sys/firmware/beepy`:

- `led`: 0 to disable LED, 1 to enable. Write-only
- `led_red`, `led_green`, `led_blue`: set LED color intensity from 0 to 255. Write- only
- `keyboard_backlight`: set keyboard brightness from 0 to 255. Write-only
- `battery_raw`: raw numerical battery level as reported by firmware. Read-only
- `battery_volts`: battery voltage estimation. Read-only
- `battery_percent`: battery percentage estimation. Read-only

## Module parameters

Write to `/sys/module/beepy_kbd/parameters/<param>` to set, or unload and reload the module with `beepy-kbd param=val`.

- `touch_act`: one of `click` or `always`
  - `click`: default, will disable touchpad until the touchpad button is clicked
  - `always`: touchpad always on, swiping sends touch input, clicking sends Enter
- `touch_as`: one of `keys` or `mouse`
  - `keys`: default, send arrow keys with the touchpad
  - `mouse`: send mouse input (useful for X11)
- `touch_shift`: default on. Send touch input while the Shift key is held
- `sharp_path`: Sharp DRM device to send overlay commands. Default: `/dev/dri/card0`

## Custom Keymap

The Alt and Sym keymaps and the Tmux prefix sent by the "Berry" key can be edited in the file `/usr/share/kbd/keymaps/beepy-kbd.map`. To reapply the keymap without rebooting, run `loadkeys /usr/share/kbd/keymaps/beepy-kbd.map`.

## Firmware update

Starting with Beepy firmware 3.0, firmware is loaded in two stages.

The first stage is a modified version of [pico-flashloader](https://github.com/rhulme/pico-flashloader).
It allows updates to be flashed to the second stage firmware while booted.

The second stage is the actual Beepy firmware.

Firmware updates are flashed by writing to `/sys/firmware/beepy/fw_update`:

- Header line beginning with `+` e.g. `+Beepy`
- Followed by the contents of an image in Intel HEX format

If the update completes successfully, the system will be rebooted.
There is a delay configurable at `/sys/firmware/beepy/shutdown_grace` to allow the operating system to cleanly shut down before the Pi is powered off.
The firmware is flashed right before the Pi boots back up, so please wait until the system reboots on its own before removing power.

If the update failed, an error will be returned and the firmware will not be modified. Check `dmesg | tail` for more information.

## Pre-requisites

- Software Pre-requisites

  1. The Raspberry Pi must have kernel drivers installed on it.
     ```bash
     sudo apt install raspberrypi-kernel-headers
     ```
  2. The Pi must have its I2C Peripheral enabled. Run `sudo raspi-config`, then under `Interface Options`, enable `I2C`.

- Hardware Pre-rquisties: Keyboard to Raspberry Pi Hardware connections.
   0. If using the keyboard without interrupt pin (How to do so is discussed below under `Installation Options`), make the following connections:
      ```
      3.3V on Pi --> 3.3V on Keyboard Module.```
      ```GND on Pi --> GND on Keyboard Module.```
      ```SCL(Pi Pin 5/BCM GPIO 3) on Pi --> SCL on Keyboard Module.```
      ```SDA(Pi Pin 3/BCM GPIO 2) on Pi --> SDA on Keyboard Module.``` 
   1. If Interrupt Pin is used, the pin is identified from the BCM Pin Number and not the Raspberry Pi GPIO Pin number.

  Check pre-requisites with the command `i2cdetect -y 1`(one might need `sudo`). They keyboard should be on the address `0x1F`. If you are using the [Keyboard FeatherWing](https://www.tindie.com/products/arturo182/keyboard-featherwing-qwerty-keyboard-26-lcd/), the Touch sensor on the screen will also show up on `0x4B`.

## Installation

To build, install, and enable the kernel module:

	make
	sudo make install

To remove:

	sudo make uninstall

## Customisation

- The Settings of the LCD can be changed as shown in [fbcp-ili9341](https://github.com/juj/fbcp-ili9341.git). Notably, the Data_Command Pin can be changed and Clock speed can be increased.
- Look at the `beepy-kbd.map` file for alternate key binds. Edit it as per your need and put the new map file in correct folder (as per `installer.sh`) to be loaded correctly in `/etc/default/keyboard`. One can use `sudo loadkeys path/to/beepy-kbd.map` while the driver is running, and press keys to check their new key bindings before placing it in the right location. Once satisfied with your keymap, run `./remover.sh &&  ./installer.sh` to reinstall everything correctly.

## Cleaning old drivers

The `bbqX0kbd` driver has been renamed to `beepy-kbd`, and `sharp` to `sharp-drm`.

Driver packages will detect if one of these old modules is installed and cancel installation of the package.

Remove the following files:

* `/lib/modules/<uname>/extra/bbqX0kbd.ko*`
* `/lib/modules/<uname>/extra/sharp.ko*`
* `/boot/overlays/i2c-bbqX0kbd.dtbo`
* `/boot/overlays/sharp.dtbo`

Rebuild the module list:

* `depmod -a`

Remove the following lines from `/boot/config.txt`:

* `dtoverlay=bbqX0kbd,irq_pin=4`
* `dtoverlay=sharp`

Remove the following lines from `/etc/modules`:

* `bbqX0kbd`
* `sharp`

## Contributing
Pull requests are welcome. 

## Special thanks
 - [arturo182](https://github.com/arturo182) & [Solder Party](https://www.solder.party/community/) for the amazing product. Please buy more of their new awesome stuff and join their discord for amazing upcoming stuff.
 - [juj's](https://github.com/juj) `frcp-ili9341` project for making small size screens so easy to use! 


## License
The code itself is LGPL-2.0 though I'm not going to stick it to you, I just chose what I saw, if you want to do awesome stuff useful to others with it without harming anyone, go for it and have fun just as I did.
