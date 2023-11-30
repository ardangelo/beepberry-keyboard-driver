# Beepy keyboard driver

Originally bbqX0kbd

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

## Modifications

- Supports sticky modifier keys. Must be used with the corresponding RP2040 firmware
  - Holding a modifier key (shift, physical alt, Symbol) while typing an alpha keys will apply the modifier to all alpha keys until the modifier is released.
  - One press and release of the modifier will enter sticky mode, applying the modifier to the next alpha key only. If the same modifier key is pressed and released again in sticky mode, it will be canceled.
- Call is mapped to Control
- Berry key is mapped to Tmux prefix (customize the prefix in the keymap file)
- Touchpad click enters Meta mode (more on this later). Double click enters touchpad scroll mode
- Back is mapped to Escape
- Holding End Call runs the power-off routine on the RP2040, but does not send an actual key to Linux
- Physical Alt is mapped to symbols via the keymap file
- Symbol is mapped to AltGr (Right Alt), mapped to more symbols via the keymap file
- Physical Alt + Enter is mapped to Tab

Integrates with [Sharp DRM driver](https://github.com/ardangelo/sharp-drm-driver)

- Draws visual mode indicators over display contents
- Indicators for Shift, Phys. Alt, Control, Alt, Symbol, Meta mode
- Meta mode key 0 toggles display color inversion

Adds the following sysfs entries under `/sys/firmware/beepy`:

- `led`: 0 to disable LED, 1 to enable. Write-only.
- `led_red`, `led_green`, `led_blue`: set LED color intensity from 0 to 255. Write-
only.
- `keyboard_backlight`: set keyboard brightness from 0 to 255. Write-only.
- `battery_raw`: raw numerical battery level as reported by firmware. Read-only.
- `battery_volts`: battery voltage estimation. Read-only.
- `rewake_timer`: send a shutdown signal to the Pi and turn back on in this many minutes. Write-only.
- `startup_reason`: cause of Pi boot (firmware init, power button, rewake timer)

Module parameters:

Write to `/sys/module/beepy_kbd/parameters/<param>` to set, or unload and
reload the module with `beepy-kbd param=val`.

- `touchpad`: one of `meta` or `keys`
  - `meta`: default, will use the touchpad button to enable or disable meta mode.
    See section below for how to use meta mode.
  - `keys`: touchpad always on, swiping sends arrow keys, clicking sends Enter.
- `shutdown_grace`: numeric 5-255
  - Wait this many seconds between a driver-initiated shutdown
    and hard power off to the Pi.
  - If greater than `rewake_timer`, a shutdown will not be triggered when
    `rewake_timer` is written.
  - Default: 30 seconds. Minimum: 5 seconds

### Meta mode

Meta mode is a modal layer that assists in rapidly moving the cursor and scrolling
with single keypresses.
To enter meta mode, click the touchpad button once. Then, the following keymap is applied, staying in
meta mode until dismissed:

- E: up, S: down, W: left, D: right. Why not WASD? This way, you can place your thumb in the middle of  all four of these keys, and more fluidly move the cursor without mistyping.
- R: Home, F: End, O: PageUp, P: PageDown
- Q: Alt+Left (back one word), A: Alt+Right (forward one word)
- T: Tab (dismisses meta mode)
- X: Apply Control to next key (dismisses meta mode)
- C: Apply Alt to next key (dismisses meta mode)
- 0: Toggle Sharp display inversion
- N: Decrease keyboard brightness
- M: Increase keyboard brightness
- $: Toggle keyboard backlight
- Touchpad click (while in meta mode): Enable touchpad scroll mode (up and down arrrow keys). Meta mode  keys will continue to work as normal. Exiting meta mode will also exit touchpad scroll mode. Subsequent  clicks of the touchpad will type Enter.
- Esc: (Back button): exit meta mode

Typing any other key while in meta mode will exit meta mode and send the key as if it was typed normally.

## Pre-requisites, Download, and Installation.

### Pre-requisites

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

### Installation

To build, install, and enable the kernel module:

	make
	sudo make install

To remove:

	sudo make uninstall

## Customisation


- The Settings of the LCD can be changed as shown in [fbcp-ili9341](https://github.com/juj/fbcp-ili9341.git). Notably, the Data_Command Pin can be changed and Clock speed can be increased.
- Look at the `beepy-kbd.map` file for alternate key binds. Edit it as per your need and put the new map file in correct folder (as per `installer.sh`) to be loaded correctly in `/etc/default/keyboard`. One can use `sudo loadkeys path/to/beepy-kbd.map` while the driver is running, and press keys to check their new key bindings before placing it in the right location. Once satisfied with your keymap, run `./remover.sh &&  ./installer.sh` to reinstall everything correctly.

## Contributing
Pull requests are welcome. 

## Special thanks
 - [arturo182](https://github.com/arturo182) & [Solder Party](https://www.solder.party/community/) for the amazing product. Please buy more of their new awesome stuff and join their discord for amazing upcoming stuff.
 - [juj's](https://github.com/juj) `frcp-ili9341` project for making small size screens so easy to use! 


## License
The code itself is LGPL-2.0 though I'm not going to stick it to you, I just chose what I saw, if you want to do awesome stuff useful to others with it without harming anyone, go for it and have fun just as I did.
