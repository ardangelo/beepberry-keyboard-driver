---
title: Beepy Keyboard Driver
layout: default
---

# Beepy Keyboard Driver

- [User Guide](#user-guide)
    - [Indicators](#indicators)
    - [Basic key mappings](#basic-key-mappings)
    - [Sticky modifier keys](#sticky-modifier-keys)
    - [Meta mode](#meta-mode)
    - [Touchpad mode](#touchpad-mode)
    - [`sysfs` interface](#sysfs-interface)
    - [Module parameters](#module-parameters)
    - [Custom keymap](#custom-keymap)
- [Developer Reference](#developer-reference)
    - [Building from source](#building-from-source)
    - [Direct write firmware updates](#direct-write-firmware-updates)

## User Guide

The Beepy keyboard driver, `beepy-kbd`, controls interaction between the [RP2040 firmware `beepy-fw`](beepy-fw.html) and Linux running on the Raspberry Pi. It has several responsibilities, including

* Communicating keyboard and touchpad input to Linux
* Power and standby management
* Synchronizing the real-time clock
* Firmware update management

### Indicators

Due to the limited number of keys, there are different shortcuts and modes mapped by the keyboard driver. You will see different types of indicators in the top right corner of the screen depending on the mode:

* <img src="assets/kbd-shift.png" width="14" alt="Shift indicator"> [Shift modifier](#sticky-modifier-keys)
* <img src="assets/kbd-phys-alt.png" width="14" alt="Physical Alt indicator"> [Physical Alt modifier](#sticky-modifier-keys)
* <img src="assets/kbd-control.png" width="14" alt="Control indicator"> [Control modifier](#basic-key-mappings)
* <img src="assets/kbd-alt.png" width="14" alt="Alt indicator"> [Alt modifier](#key-mappings)
* <img src="assets/kbd-altgr.png" width="14" alt="Symbol indicator"> [Symbol mode modifier](#alt-and-sym-modifiers)
* <img src="assets/kbd-meta.png" width="14" alt="Meta mode indicator"> [Meta mode modifier](#meta-mode)
* <img src="assets/kbd-touch.png" width="14" alt="Touchpad mode indicator"> [Touchpad mode modifier](#touchpad-mode)

### Basic key mappings

From left to right, the top row of the Beepy keyboard has the following keys:

* "Call", a phone facing up.
    * Single click: Enter Control key ([sticky modifier](#sticky-modifier-keys)).
    * Short hold: Lock Control key ([sticky modifier](#sticky-modifier-keys)).
* "Berry": a collection of sections shaped like a fruit.
    * Single click: Enter [Meta mode](#meta-mode).
    * Short hold (1s): Display [Meta mode reference overlay](#meta-mode).
* "Touchpad": pressing the touchpad sensor will click and produce a key event.
    * Single click: by default, enable [touchpad mode](#touchpad-mode), sending arrow keys. If touchpad mode is already on, a single click will send `Enter`.
* "Back": an arrow looping back onto itself.
    * Single click: send `Escape`. Commonly used to exit menus in utilities.
* "End Call": a phone facing down, with a line underneath.
    * Single click: Send the [tmux prefix](beepy-tmux-menus.html). Prefix is [configurable in the driver keymap](#custom-keymaps).
    * Short hold (1s): Open the [tmux menu](beepy-tmux-menus.html). If the Pi has been shut down, a short hold will turn the Pi back on.
    * Long hold (5s): send a shutdown signal to the Pi.

The alternate symbols printed directly on the keys are sent by pressing the `Physical Alt` key on the bottom left corner of the keyboard, then pressing the key on which the desired symbol is printed. While `Physical Alt` is active, you will see an `a` indicator in the top right corner of the screen: <img src="assets/kbd-phys-alt.png" width="14" alt="Physical Alt indicator">. The combination `Physical Alt` + `Enter` is also mapped to `Tab`. `Physical Alt` is a "[sticky modifier key](#sticky-modifier-keys)".

For additional symbols not printed directly on the keys, use the `Symbol` key on the bottom row of the keyboard. While `Symbol` is active, you will see an `S` indicator in the top right of the screen: <img src="assets/kbd-altgr.png" width="14" alt="Symbol indicator">. Internally, `Symbol` sends AltGr (Right Alt), which is mapped to more symbols via the keymap file at `/usr/share/kbd/keymaps/beepy-kbd.map`. `Symbol` is a "[sticky modifier key](#sticky-modifier-keys)". You can view the Symbol key map by holding the `Symbol` key for 1 second. Modifying the keymap file will also update the Symbol key map displayed; below is the default key map:

![Default Symbol key map](assets/overlay-symbol.png)

Arrow keys and other movement keys such as Page Up and Page Down are accessible in [Meta mode](#meta-mode).

### Sticky modifier keys

For easier typing, the keyboard driver implements sticky modifier keys. Pressing and releasing a modifier applies the modifier to the next alpha keypress only. If the same modifier key is pressed and released again, it will be canceled.

Holding a modifier key while typing an alpha key will apply the modifier to all alpha keys until the modifier is released.

While a modifier key is active, [visual indicators](#indicators) are drawn in the top right corner of the display, with indicators for <img src="assets/kbd-shift.png" width="14" alt="Shift indicator"> [Shift](#sticky-modifier-keys), <img src="assets/kbd-phys-alt.png" width="14" alt="Physical Alt indicator"> [Physical Alt](#sticky-modifier-keys), <img src="assets/kbd-control.png" width="14" alt="Control indicator"> [Control](#basic-key-mappings), <img src="assets/kbd-alt.png" width="14" alt="Alt indicator"> [Alt](#key-mappings), <img src="assets/kbd-altgr.png" width="14" alt="Symbol indicator"> [Symbol](#alt-and-sym-modifiers), <img src="assets/kbd-meta.png" width="14" alt="Meta mode indicator"> [Meta mode](#meta-mode), and <img src="assets/kbd-touch.png" width="14" alt="Touchpad mode indicator"> [Touchpad mode](#touchpad-mode).

For example, to type a dot `.`, you can use `Physical Alt` sticky mode:

* Press and release the `Physical Alt` key
* The `a` indicator <img src="assets/kbd-phys-alt.png" width="14" alt="Physical Alt indicator"> appears, showing that `Physical Alt` mode will be applied to the next keypress only
* Type the `M` key to input a dot `.`
* The `a` indicator disappears

Alternatively, to type a dot `.` followed by a comma `,`, you can also press and hold `Physical Alt`:

* Press the `Physical Alt` key
* The `a` indicator <img src="assets/kbd-phys-alt.png" width="14" alt="Physical Alt indicator"> appears, showing that `Physical Alt` mode is active
* While holding `Physical Alt`, type the `M` key to input a dot `.`
* While holding `Physical Alt`, type the `N` key to input a comma `,`
* Release the `Physical Alt` key
* The `a` indicator disappears

### Meta mode

Meta mode is a modal layer that assists in rapidly moving the cursor and scrolling with single keypresses. To enter Meta mode, click the `Berry` key once. The Meta mode indicator <img src="assets/kbd-meta.png" width="14" alt="Meta mode indicator"> will appear in the top right corner of the screen, and the following keymap will be applied until Meta mode is dismissed using the `Back` key, or otherwise noted:

- `E` Up `S` Down `W` Left `D` Right
    - Why not `WASD`? This way, you can place your thumb in the middle of all four of these keys, and fluidly move the cursor without mistyping on e.g. `Q` or `E`
- `R` Home `F` End `O` Page Up `P` Page Down
- `Q` Back one word (`Alt + Left`) `A` Forward one word (`Alt + Right`)
- `T` Tab (dismisses Meta mode)
- `X` Apply `Control` to next key (dismisses Meta mode)
- `C` Apply `Alt` to next key (dismisses Meta mode)
- `0` Toggle display inversion from white-on-black to black-on-white
- `N` Decrease keyboard brightness
- `M` Increase keyboard brightness
- `$` Toggle keyboard backlight
- `Back` Exit Meta mode

Typing any other key while in Meta mode will exit Meta mode and send the key as if it was typed normally.

You can view the Meta mode key map by holding the `Berry` key for 1 second:

![Meta mode key map](assets/overlay-meta.png)

### Touchpad mode

The Beepy touchpad is not actually touch sensitive, rather it is an optical trackpad. This has the downside of sending touchpad input if material other than a finger moves across it, such as a pocket. To reduce false positives, the default mode of the Beepy touchpad is to remain off until a key is used to turn it on. Touchpad behavior, including mouse and activation, can be configured via [module parameters](#module-parameters).

Press the touchpad itself to turn on touchpad mode, and start sending arrow keys when you move your finger across the touchpad. While active, you will see the touchpad indicator <img src="assets/kbd-touch.png" width="14" alt="Touchpad indicator"> in the top-right corner of the screen.

Clicking the touchpad itself again while the touchpad is active will send `Enter`. Pressing the `Back` key will exit touchpad mode.

You can also hold the `Shift` key to temporarily turn on the touchpad until the `Shift` key is released. You will see the Shift indicator <img src="assets/kbd-shift.png" width="14" alt="Shift indicator"> instead of the touch indicator.

If you release the `Shift` key *without* using the touchpad, you will instead get the [sticky modifier behavior](#sticky-modifier-keys) of applying Shift to the next alpha keypress. In this case, the Shift indicator will remain on the screen. Press and release the `Shift` key again to un-stick the modifier and hide the indicator.

### `sysfs` interface

The keyboard driver creates several sysfs entries under `/sys/firmware/beepy` to expose different parts of the firmware. These entries can be manipulated like a normal file using traditional Unix tools such as `cat` and `tee`, and in shell scripts.

* `battery_raw` Raw ADC output value for the battery level. Read-only.
* `battery_volts` Approximate battery voltage. Read-only.
* `battery_percent` Approximate battery percentage remaining. Read-only.
* `led_red`, `led_green`, `led_blue` set LED color intensity from 0 to 255. Apply by writing to `led`. Write-only.
* `led`: Also applies color settings. Write-only.
  - `0` Turn off LED.
  - `1` Turn on LED.
  - `2` Flash LED.
  - `3` Flash LED until key pressed. Overlays on top of an existing LED setting. For example, the following write sequence will set the LED to be solid red, but with a blue flash. Then, when a key is pressed, it will return to a solid red.

        # Set LED to solid red
        echo 255 | sudo tee /sys/firmware/beepy/led_red
        echo   0 | sudo tee /sys/firmware/beepy/led_green
        echo   0 | sudo tee /sys/firmware/beepy/led_blue
        echo   1 | sudo tee /sys/firmware/beepy/led

        # Set LED to flash blue until key pressed
        echo   0 | sudo tee /sys/firmware/beepy/led_red
        echo   0 | sudo tee /sys/firmware/beepy/led_green
        echo 255 | sudo tee /sys/firmware/beepy/led_blue
        echo   3 | sudo tee /sys/firmware/beepy/led

* `keyboard_backlight` Set keyboard brightness from 0 to 255. Write-only. The default brightness is low, but still on. Setting the brightness to maximum will noticeably increase power draw.
* `rewake_timer` Write to shut down the Pi, then power-on in that many minutes. Write-only. Useful for polling services in conjunction with `startup_reason`, such as with the [beepy-poll](beepy-poll.html) service.
* `startup_reason` Contains the reason why the Pi was booted. Useful for polling services in conjunction with `rewake_timer`.
    - `fw_init` RP2040 initialized and booted Pi.
    - `power_button` Power button held to turn Pi back on.
    - `rewake` Rewake triggered from `rewake_timer`.
    - `rewake_canceled` During rewake polling, 0 was written to `rewake_timer`. This allows the `beepy-poll` service to cancel the poll and proceeded with a full boot.
* `fw_version` Installed firmware version. Read-only.
* `fw_update` Write to update firmware. Read-write See [Firmware updates](#firmware-updates).
* `last_keypress` Milliseconds since last keypress. Read-only.

### Module parameters

Configure various aspects of the driver itself. Write to `/sys/module/beepy_kbd/parameters/<param>` to set with `echo <val> | sudo tee /sys/module/beepy_kbd/parameters/<param>`. Or unload and reload the module with `sudo modprobe -r beepy-kbd; sudo modprobe beepy-kbd param=val`.

* `touch_act` One of `click` or `always`.
  - `click` Default, will disable touchpad until the touchpad button is clicked.
  - `always` Touchpad always on, swiping sends touch input, clicking sends `Enter`.
* `touch_as`: one of `keys` or `mouse`.
  - `keys` Default, send arrow keys with the touchpad.
  - `mouse` Send mouse input (useful for X11).
* `touch_shift` Default on. Send touch input while the Shift key is held.
* `touch_min_squal` Reject touchpad input if surface quality as reported by touchpad sensor is lower than this threshold. Default `16`.
* `touch_led_setting` One of `low`, `med`, `high`. Touchpad LED power setting. `high` is recommended for reliable input. Default `high`.
* `touch_threshold` Touchpad movement amount required to send arrow key. Range `0 - 255`, default `8`.
- `shutdown_grace` To avoid powering off the Pi while it is still running, this is set to the number of seconds to wait between a shutdown signal and the firmware removing power from the Pi. This helps ensure that the Pi has time to process the power-off command and to shut down cleanly. Default `30` seconds.
- `auto_off` In most cases, the keyboard driver is loaded on boot and unloaded during shutdown. For substantial power savings, the default-enabled `auto_off` setting will trigger when the driver is unloaded. After a 30 second wait to allow for the driver to potentially be reloaded, the Pi will shutdown, wait for `shutdown_grace` seconds, then power off the Pi and enter deep sleep. Default on.
* `sharp_path` Sharp DRM device to send overlay commands. Default: `/dev/dri/card0`.
* `sysfs_gid` Group ID of sysfs entries in `/sys/firmware/beepy`. Set this to the result of `id -g` to allow access without `sudo`. Beepy Raspbian configures the first user group by default.
* `handle_poweroff` Enable to have driver invoke `/sbin/poweroff` when power key held. not necessary for Beepy Raspbian, may be necessary if running a custom build of the driver on another Linux distribution. Default off.

### Custom keymap

The `Physical Alt` and `Symbol` keymaps and the Tmux prefix sent by the `Berry` key can be edited in the file `/usr/share/kbd/keymaps/beepy-kbd.map`. To reapply the keymap without rebooting, run `loadkeys /usr/share/kbd/keymaps/beepy-kbd.map`.

Holding the `Symbol` key to display the keymap will display the keymap directly from this file.

## Developer Reference

### Building from source

* Install build prerequisites.

    sudo apt-get install raspberrypi-kernel-headers

* Enable I2C Peripheral. Run `sudo raspi-config`, then under `Interface Options`, enable `I2C`.

* Build, install, and enable the kernel module.

	make
	sudo make install

To remove, run `sudo make uninstall`, then verify that `/etc/modules` and `/boot/firmware/config.txt` have removed the driver lines.

### Direct write firmware updates

In most cases, you will use the [`update-beepy-fw` utility](beepy-fw.html#firmware-update-utility) to update firmware from a firmware package or file. However, you can also manually write firmware update data to the sysfs entry at `/sys/firmware/beepy/fw_update`.

RP2040 firmware is loaded in two stages. The first stage is a modified version of [pico-flashloader](https://github.com/rhulme/pico-flashloader). It allows updates to be flashed to the second stage firmware while booted. The second stage is the actual Beepy firmware.

Firmware updates are flashed by writing to `/sys/firmware/beepy/fw_update`:

- Header line beginning with `+` e.g. `+Beepy`
- Followed by the contents of an image in Intel HEX format

Please wait until the system reboots on its own before removing power. If the update failed,  will contain an error code and the firmware will not be modified.

The header line `+...` will reset the update process, so an interrupted or failed update can be retried by restarting the firmware write.

If the update completes successfully, the system will be rebooted.
There is a delay configurable at `/sys/firmware/beepy/shutdown_grace` to allow the operating system to cleanly shut down before the Pi is powered off.
The firmware is flashed right before the Pi boots back up, so please wait until the system reboots on its own before removing power.
