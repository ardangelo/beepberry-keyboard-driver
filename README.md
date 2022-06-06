# BBQX0KBD

BBQX0KBD is an I2C Keyboard Device Driver for keyboard devices made by [Solder Party](https://www.solder.party/community/).  The following keyboards are currently supported by this driver: 
1. [BB Q10 Keyboard PMOD](https://www.tindie.com/products/arturo182/bb-q10-keyboard-pmod/)
2. [Keyboard FeatherWing](https://www.tindie.com/products/arturo182/keyboard-featherwing-qwerty-keyboard-26-lcd/)
3. [BBQ20KBD](https://www.tindie.com/products/arturo182/bb-q20-keyboard-with-trackpad-usbi2cpmod/)

**The current version supports the devices as Console Keyboards only, the desktop GUI usage has minor issues.**

The drivers are named so (with an 'X'), so as to support future products by the original creator. The driver is mainly developed and tested on the following SBCs from [Raspberry Pi Foundation's](https://www.raspberrypi.org)

1. [Raspberry Pi Zero W](https://www.raspberrypi.com/products/raspberry-pi-zero-w/)
2. [Raspberry Pi Zero 2 W](https://www.raspberrypi.com/products/raspberry-pi-zero-2-w/)
3. [Raspberry Pi 3 Model B](https://www.raspberrypi.com/products/raspberry-pi-3-model-b/). 

## Features
The original [source code](https://github.com/arturo182/bbq10pmod_module) by [arturo182](https://github.com/arturo182), is a good starting point from where this driver builds on. This driver offers the following features:

- Ability to use the keyboard without needing an extra Interrupt Signal.
- Ability to use lower and upper case letters.
- Ability to use SYM Key on Keyboard as Right Alt (or Alt Gr).
- Caps Lock and Num Lock.
- Control Keyboard and Screen* Brightness from key presses.
- Ability to assign different functionality to trackpad, either as a mouse or a single Home Button.**
- Control Volume from key presses.***
- Software shutdown of keyboard and screen backlights when driver is removed.
- Minimum effort to change I2C Peripheral address for Keyboard.

*Available only on [Keyboard FeatherWing](https://www.tindie.com/products/arturo182/keyboard-featherwing-qwerty-keyboard-26-lcd/) for now.

**Available only on [BBQ20KBD](https://www.tindie.com/products/arturo182/bb-q20-keyboard-with-trackpad-usbi2cpmod/) for now.

***Not tested this feature 100%.

## Keyboard Layout

The keyboard layout is shown below. 

The top row is unique to the [Keyboard FeatherWing](https://www.tindie.com/products/arturo182/keyboard-featherwing-qwerty-keyboard-26-lcd/), while the [BBQ20KBD](https://www.tindie.com/products/arturo182/bb-q20-keyboard-with-trackpad-usbi2cpmod/) has a similar top key row, with the essential difference that it uses a trackpad instead of 5-way key. Even then, except for the `CTRL` Key, every other keypress on this row can be executed from the keyboard/[BB Q10 Keyboard PMOD](https://www.tindie.com/products/arturo182/bb-q10-keyboard-pmod/) module itself.

By default, each key on the keyboard itself signifies the middle element mentioned in the layout below.

To access the keys mentioned above a character, press and hold `LFTALT` (Left alt) and press the desired key. For example, `Key(/)` is presented by `LFTALT`+`G`.

To access the keys mentioned below a character, press and hold `RTALT` (Right alt) and press the desired key. For example, `Key(=)` is presented by `RTALT`+`I`.

Upper Case Letters can be executed by pressing `LEFT_SHIFT` + the key.

Caps Lock is toggled by pressing `LFTALT` + `LEFT_SHIFT`.

Num Lock is toggled by pressing `LFTALT` + `RIGHT_SHIFT`.

Screen Brightness can be increased or decreased by `RTALT`+`Q` and `RTALT`+`W` respectively. Toggling the same is done by `RTALT`+`S`.

Keyboard Brightness can be increased or decreased by `RTALT`+`K` and `RTALT`+`L` respectively. Toggling the same is done by `RTALT`+`$`.

Volume can be increased or decreased by `RTALT`+`Z` and `RTALT`+`X` respectively. Muting the same is done by `RTALT`+`~`.
``` 
 *		   BBQ10KBD PMOD KEYBOARD LAYOUT
 *
 *	+------+-----+----+----+----+----+----+-----+-----+-------+
 *	|#     |1    |2   |3   |(   |   )|_   |    -|    +|      @|
 *	|  Q   |  W  | E  | R  | T  |  Y |  U |  I  |  O  |   P   |
 *	|    S+|   S-|PgUp|PgDn|   \|UP  |^   |=    |{    |}      |
 *	+------+-----+----+----+----+----+----+-----+-----+-------+
 *	|*     |4    |5   |6   |/   |   :|;   |    '|    "|    ESC|
 *	|  A   |  S  | D  | F  | G  |  H |  J |  K  |  L  |  BKSP |
 *	|     ?|   Sx|   [|   ]|LEFT|HOME|RGHT|V+   |V-   |DLT    |
 *	+------+-----+----+----+----+----+----+-----+-----+-------+
 *	|      |7    |8   |9   |?   |   !|,   |    .|    `|       |
 *	|LFTALT|  Z  | X  | C  | V  |  B |  N |  M  |  $  | ENTER |
 *	|      |   K+|  K-|   °|   <|DOWN|>   |MENU |Vx   |       |
 *	+------+-----+----+----+----+----+----+-----+-----+-------+
 *	|            |0   |TAB                |     |             |
 *	| LEFT_SHIFT | ~  |       SPACE       |RTALT| RIGHT_SHIFT |
 *	|            |  Kx|                  &|     |             |
 *	+------------+----+-------------------+-----+-------------+
 *
```
```
 *		BBQ10KBD FEATHERWING KEYBOARD LAYOUT
 *
 *	+------+-----+----+----+----+----+----+-----+-----+-------+
 *	|      |          |        UP         |           |       |
 *	| Ctrl |   PgDn   |  LEFT HOME RIGHT  |   PgUp    | MENU  |
 *	|      |          |       DOWN        |           |       |
 *	+------+-----+----+----+----+----+----+-----+-----+-------+
 *	|                                                         |
 *	+------+-----+----+----+----+----+----+-----+-----+-------+
 *	|#     |1    |2   |3   |(   |   )|_   |    -|    +|      @|
 *	|  Q   |  W  | E  | R  | T  |  Y |  U |  I  |  O  |   P   |
 *	|    S+|   S-|PgDn|PgUp|   \|UP  |^   |=    |{    |}      |
 *	+------+-----+----+----+----+----+----+-----+-----+-------+
 *	|*     |4    |5   |6   |/   |   :|;   |    '|    "|    ESC|
 *	|  A   |  S  | D  | F  | G  |  H |  J |  K  |  L  |  BKSP |
 *	|     ?|   Sx|   [|   ]|LEFT|HOME|RGHT|V+   |V-   |DLT    |
 *	+------+-----+----+----+----+----+----+-----+-----+-------+
 *	|      |7    |8   |9   |?   |   !|,   |    .|    `|       |
 *	|LFTALT|  Z  | X  | C  | V  |  B |  N |  M  |  $  | ENTER |
 *	|      |   K+|  K-|   °|   <|DOWN|>   |MENU |Vx   |       |
 *	+------+-----+----+----+----+----+----+-----+-----+-------+
 *	|            |0   |TAB                |     |             |
 *	| LEFT_SHIFT | ~  |       SPACE       |RTALT| RIGHT_SHIFT |
 *	|            |  Kx|                  &|     |             |
 *	+------------+----+-------------------+-----+-------------+
 *
```
```
 *	               BBQ20KBD PMOD KEYBOARD LAYOUT
 *
 *	+------+-----+----+----+----+----+----+-----+-----+-------+
 *	|      |          |BR     ↑TPY-       |           |       |
 *	| Ctrl |   PgDn   |←TPX- BL(HOME)TPX+→|   PgUp    | MENU  |
 *	|      |          |       ↓TPY+       |           |       |
 *	+------+-----+----+----+----+----+----+-----+-----+-------+
 *	|                                                         |
 *	+------+-----+----+----+----+----+----+-----+-----+-------+
 *	|#     |1    |2   |3   |(   |   )|_   |    -|    +|      @|
 *	|  Q   |  W  | E  | R  | T  |  Y |  U |  I  |  O  |   P   |
 *	|      |     |PgDn|PgUp|   \|UP  |^   |=    |{    |}      |
 *	+------+-----+----+----+----+----+----+-----+-----+-------+
 *	|*     |4    |5   |6   |/   |   :|;   |    '|    "|    ESC|
 *	|  A   |  S  | D  | F  | G  |  H |  J |  K  |  L  |  BKSP |
 *	|     ?|     |   [|   ]|LEFT|HOME|RGHT|V+   |V-   |DLT    |
 *	+------+-----+----+----+----+----+----+-----+-----+-------+
 *	|      |7    |8   |9   |?   |   !|,   |    .|    `|       |
 *	|LFTALT|  Z  | X  | C  | V  |  B |  N |  M  |  $  | ENTER |
 *	|      |   K+|  K-|   °|   <|DOWN|>   |MENU |Vx   |       |
 *	+------+-----+----+----+----+----+----+-----+-----+-------+
 *	|            |0   |TAB                |     |             |
 *	| LEFT_SHIFT | ~  |       SPACE       |RTALT| RIGHT_SHIFT |
 *	|            |  Kx|                  &|     |             |
 *	+------------+----+-------------------+-----+-------------+
 *
```


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

### Download

Copy the Git repository from above and download it on your Pi, preferably on `/home/pi/` using the below command and change in to the folder.
```bash
git clone https://github.com/wallComputer/bbqX0kbd_driver.git
cd bbqX0kbd_driver/
```

### Installation
- To install the files with default configurations, use the `installer.sh` file.
  ```bash
  ./installer.sh --OPTION VALUE
  ```
  Where `--OPTION` is  valid configuration option discussed below.
- To remove, use the `remover.sh` file.
  ```bash
  ./remover.sh
  ```
## Installtion Options
The installation script `installer.sh` has multiple options. The description of these can be accessed by `-h` or `--h` option as below
```bash
  ./installer.sh -h
```
The following options are available:
- `--BBQX0KBD_TYPE` : Keyboard Version You want to build kernel driver for.
Available Options:
   1. BBQ10KBD_PMOD
   2. BBQ10KBD_FEATHERWING
   3. BBQ20KBD_PMOD (Default)
- `--BBQ20KBD_TRACKPAD_USE` : Use BBQ20KBD_PMOD's  trackpad either as a mouse, or as KEY_HOME.
Availabe Options:
   1. BBQ20KBD_TRACKPAD_AS_MOUSE (Default)
   2. BBQ20KBD_TRACKPAD_AS_KEYS
- `--BBQX0KBD_INT` : Build a driver that uses the external Interrupt Pin, or build one without it, polling the Keyboard instead.
Available Options:
   1. BBQX0KBD_USE_INT
   2. BBQX0KBD_NO_INT (Default)
- `--BBQX0KBD_INT_PIN` : Raspberry Pi BCM Pin Number for the Interrupt Pin. Default value is 4. Note that BCM Pin Numbers are different from Pi's header Pin Numbers.
- `--BBQX0KBD_POLL_PERIOD` : Polling Rate for the Keyboard in milliseconds. Value between 10 to 1000. Defualt is 40.
- `--BBQX0KBD_ASSIGNED_I2C_ADDRESS` : I2C Address if changed. Default Value is 0x1F.

The following examples may be useful:

```bash
./installer.sh
```
This is the default option. It creates a Keyboard Driver for BBQ20KBD on I2C Address `0x1F`. The Keyboard does not need Int Pin and instead polls the Keyboard every 40ms. The trackpad is used as a mouse, and the button is used as mouse left click. On pressing the button with Left Alt as `LFTALT + BTN_LEFT`, a mouse right key is registered.

```bash
./installer.sh --BBQX0KBD_TYPE BBQ10KBD_FEATHERWING --BBQX0KBD_INT BBQX0KBD_NO_INT --BBQX0KBD_POLL_PERIOD 25
```
This creates the kernel driver for Keyboard Featherwing. It does not use the Interrupt Pin, instead polls the keyboard every 25ms.

```bash
./installer.sh --BBQX0KBD_TYPE BBQ10KBD_PMOD --BBQX0KBD_INT BBQX0KBD_USE_INT --BBQX0KBD_INT_PIN 23
```
This creates the kernel driver for BBQ10KBD_PMOD. It does use the Interrupt Pin, namely, BCM Pin GPIO 23 (Pin 16 on the header).

```bash
./installer.sh --BBQX0KBD_TYPE BBQ20KBD_PMOD --BBQX0KBD_INT BBQX0KBD_NO_INT --BBQX0KBD_ASSIGNED_I2C_ADDRESS 0x45
```
This creates the kernel driver for BBQ20KBD_PMOD. The Driver assumes the Keyboard is available on I2C Address 0x45, as well as polls it at default poll rate of 40ms.

```bash
./installer.sh --BBQX0KBD_TYPE BBQ20KBD_PMOD --BBQ20KBD_TRACKPAD_USE BBQ20KBD_TRACKPAD_AS_KEYS
```
This creates the kernel driver for BBQ20KBD_PMOD and uses the trackpad as KEY_HOME. The Mouse movement is not registered.

After following guide, reboot your pi and on login, the Keyboard should be working on `/dev/tty1`. To view the above console, Use an LCD such as the one on the [Keyboard FeatherWing](https://www.tindie.com/products/arturo182/keyboard-featherwing-qwerty-keyboard-26-lcd/), or any other you can get to work with your Pi. The easiest way is to either use a Adafruit PiTFT, or use the below example of using [juj's](https://github.com/juj) `frcp-ili9341` project.

###(Optional) Setting Up a Screen with the Keyboard Module

[Keyboard FeatherWing](https://www.tindie.com/products/arturo182/keyboard-featherwing-qwerty-keyboard-26-lcd/) has an ILI9341 on it. Using it for the main console can be of great use. The below steps describe how to connect and control this LCD from the Pi.

  1. Hardware connections
     ```bash
     3.3V on Pi ----> 3.3V on Keyboard FeatherWing.
     GND on Pi -----> GND on Keyboard FeatherWing.
     SCLK(Pi Pin 23/BCM GPIO 11) ---> SCK on Keyboard FeatherWing.
     MOSI(Pi Pin 19/BCM GPIO 10) ---> CO on Keyboard FeatherWing.
     CE0(Pi Pin 24/BCM GPIO 8) ---> LCD_CS on Keyboard FeatherWing.
     GPIO9(Pi Pin 13/BCM GPIO 27) ---> LCD_DC on Keyboard FeatherWing.
     ```
     If you are using the [Keyboard FeatherWing](https://www.tindie.com/products/arturo182/keyboard-featherwing-qwerty-keyboard-26-lcd/), you will also need to pull the `RST` Pin on the Featherwing to `3.3V`. 
     ```bash
     3.3V on Pi ----> RST on Keyboard FeatherWing.
     ```
  2. Install git and cmake.
     ```bash
     sudo apt install git cmake
     ```
  3. Clone [juj's](https://github.com/juj) project, and create a directory `build` inside it, and change in to it.
     ```bash
     git clone https://github.com/juj/fbcp-ili9341.git
     cd fbcp-ili9341
     mkdir build
     cd build/
     ```
  4. Inside the `build` directory, run the below `cmake` command.
     ```bash
     cmake -DILI9341=ON -DSPI_BUS_CLOCK_DIVISOR=40 -DGPIO_TFT_DATA_CONTROL=27 -DSTATISTICS=0 ..
     ```
  5. Run a make command as
     ```bash
     make -j
     ```
  6. The result of the above file will be a `fbcp-ili9341` file. Place the absolute path of this file in the **last line but one** of `/etc/rc.local` as below **(assuming the absolute path is correct)**
     ```bash
     sudo /home/pi/fbcp-ili9341/build/fbcp-ili9341 &
     ```
  7. Place the below text in /boot/config.txt
     ```bash
     hdmi_group=2
     hdmi_mode=87
     hdmi_cvt=320 240 60 1 0 0 0
     hdmi_force_hotplug=1
     ```
  8. From `sudo raspi-config`, under `System Options`, set the `Boot / Auto Login` feature to `Console` or `Console Autologin`. 
  9. Reboot.
  10. On boot up, the LCD should start showing console. It might be useful to run `sudo dpkg-reconfigure console-setup` (can be done over ssh), choose the desired font and size, and place a `sudo /etc/init.d/console-setup.sh reload &` in the `/etc/rc.local` file just as in step 5.

## Customisation


- The Settings of the LCD can be changed as shown in [fbcp-ili9341](https://github.com/juj/fbcp-ili9341.git). Notably, the Data_Command Pin can be changed and Clock speed can be increased.
- Look at the `bbqX0kbd.map` file for alternate key binds. Edit it as per your need and put the new map file in correct folder (as per `installer.sh`) to be loaded correctly in `/etc/default/keyboard`. One can use `sudo loadkeys path/to/bbqX0kbd.map` while the driver is running, and press keys to check their new key bindings before placing it in the right location. Once satisfied with your keymap, run `./remover.sh &&  ./installer.sh` to reinstall everything correctly.

## Known Issues

  - The BIGGEST FLAW at the moment is that the driver does not work as intended on Desktop view. The basic keys work, so do Num Lock and Caps Lock features. However, the mapping assigned as per the above keyboard layout does not present itself. Pressing the `LFTALT` key immediately presents the alternate optional menus, which hinders getting any higher and lower keys as per the layout. Two things are suspected, either the Buster Keyboard Layout configuration has some issues (Buster has had issues in Keyboard config for me from `raspi-config`, or else it's as intended behaviour, `Alt` Key pressed on a GUI indicates using other keys for menu options and not for printing characters. 
  - `RTALT`+`$` (or the Volume Key on [BBQ20KBD](https://www.tindie.com/products/arturo182/bb-q20-keyboard-with-trackpad-usbi2cpmod/)) produces `MUTE` Key. However on Console, it also puts out the `~` key. I have not yet been able to find the cause of this issue. 
  - The entire feature of Volume control is untested as of now. `evtest` shows the `Volume Up`, `Volume Down`, and `MUTE` keys pressed, but as the keyboard driver works the best on console only at the moment, I'm not sure how this function works on a Desktop GUI.
  - On first boot, `RTALT`+`G` or `RTALT`+`J` presents other `tty` options one can log in to, and the `RTALT`+`Y`, `RTALT`+`H`, and `RTALT`+`B` do not produce the desired keys too . This behaviour is found **only** on first boot after installing the driver, never after and the keyboard works fine from there on.
  - Rarely, the keyboard often becomes unresponsive and does not appear on the I2C Bus for extended period of time. The only solution becomes to shutdown the Pi completely (often with the need to remove power). I do not know if this is a driver issue or hardware issue, and I have not been able to replicate it often. **If you are able to replicate it, please let me know and I'll try my best to solve it in code.**

## Future Work
 - ~~Move the fifo reading task to a work queue. This will require changes in a few places, though the core logic should not change.~~ [DONE: using `delayed_work_branch`]
 - Solve the BIGGEST FLAW
 - ~~Make the code compatible with upcoming works of [Solder Party](https://www.solder.party/community/) Keyboards.~~
 - Add The touch screen sensor to Keyboard Driver itself.
 - ~~Add [Pimoroni trackball](https://shop.pimoroni.com/products/trackball-breakout) to be used as single press mouse to the screen and keyboard.~~ [Done: using different kernel driver.]
 - Solve other design flaws.
 - Use the Neopixel on the [Keyboard FeatherWing](https://www.tindie.com/products/arturo182/keyboard-featherwing-qwerty-keyboard-26-lcd/) as well as the Ambient light sensor (if possible for the latter with some cheap single channel ADC.
 - Create a PCB to make the design modular and compact.

## Contributing
Pull requests are welcome. 

## Special thanks
 - [arturo182](https://github.com/arturo182) & [Solder Party](https://www.solder.party/community/) for the amazing product. Please buy more of their new awesome stuff and join their discord for amazing upcoming stuff.
 - [juj's](https://github.com/juj) `frcp-ili9341` project for making small size screens so easy to use! 


## License
The code itself is LGPL-2.0 though I'm not going to stick it to you, I just chose what I saw, if you want to do awesome stuff useful to others with it without harming anyone, go for it and have fun just as I did.