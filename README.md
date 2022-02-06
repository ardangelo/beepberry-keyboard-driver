# BBQX0KBD

BBQX0KBD is a Keyboard Device Driver for the [BB Q10 Keyboard PMOD](https://www.tindie.com/products/arturo182/bb-q10-keyboard-pmod/) and [Keyboard FeatherWing](https://www.tindie.com/products/arturo182/keyboard-featherwing-qwerty-keyboard-26-lcd/) by [Solder Party](https://www.solder.party/community/). **The current version supports the devices as Console Keyboards only, the desktop GUI usage has issues.**

The drivers are named so (with an 'X'), so as to support future products by the original creator. The driver is mainly developed for [Raspberry Pi Foundation's](https://www.raspberrypi.org) [Raspberry Pi Zero 2 W](https://www.raspberrypi.com/products/raspberry-pi-zero-2-w/),[Raspberry Pi Zero W](https://www.raspberrypi.com/products/raspberry-pi-zero-w/), and [Raspberry Pi 3 Model B](https://www.raspberrypi.com/products/raspberry-pi-3-model-b/). 

## Features
The original [source code](https://github.com/arturo182/bbq10pmod_module) by [arturo182](https://github.com/arturo182), is a good starting point from where this driver builds on. This driver offers the following features:

- Ability to use lower and upper case letters.
- Ability to use SYM Key on Keyboard as Right Alt (or Alt Gr).
- Caps Lock and Num Lock.
- Control Keyboard and Screen* Brightness from key presses.
- Control Volume from key presses.**
- Software shutdown of keyboard and screen backlights when driver is removed.

*Available only on [Keyboard FeatherWing](https://www.tindie.com/products/arturo182/keyboard-featherwing-qwerty-keyboard-26-lcd/) for now.

**Not tested this feature 100%.

## Keyboard Layout

The keyboard layout is shown below. 

The top row is unique to the [Keyboard FeatherWing](https://www.tindie.com/products/arturo182/keyboard-featherwing-qwerty-keyboard-26-lcd/) only. Even then, except for the `CTRL` Key, every other keypress on this row can be executed from the keyboard/[BB Q10 Keyboard PMOD](https://www.tindie.com/products/arturo182/bb-q10-keyboard-pmod/) module itself.

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
 *                      KEYBOARD LAYOUT
 *	+------+-----+----+----+----+----+----+-----+-----+-------+
 *	|      |          |        UP         |           |       |
 *	| Ctrl |   PgUp   |  LEFT HOME RIGHT  |   PgDn    | MENU  |
 *	|      |          |       DOWN        |           |       |
 *	+------+-----+----+----+----+----+----+-----+-----+-------+
 *	|                                                         |
 *	+------+-----+----+----+----+----+----+-----+-----+-------+
 *	|#     |1    |2   |3   |(   |   )|_   |    -|    +|      @|
 *	|  Q   |  W  | E  | R  | T  |  Y |  U |  I  |  O  |   P   |
 *	|    S+|   S-|PgUp|PgDn|   \|UP  |^   |=    |{    |}      |
 *	+------+-----+----+----+----+----+----+-----+-----+-------+
 *	|*     |4    |5   |6   |/   |   :|;   |    '|    "|    ESC|
 *	|  A   |  S  | D  | F  | G  |  H |  J |  K  |  L  |  BKSP |
 *	|     ||   Sx|   [|   ]|LEFT|HOME|RGHT|K+   |K-   |DLT    |
 *	+------+-----+----+----+----+----+----+-----+-----+-------+
 *	|      |7    |8   |9   |?   |   !|,   |    .|    `|       |
 *	|LFTALT|  Z  | X  | C  | V  |  B |  N |  M  |  $  | ENTER |
 *	|      |   V+|  V-|   °|   <|DOWN|>   |MENU |Kx   |       |
 *	+------+-----+----+----+----+----+----+-----+-----+-------+
 *	|            |0   |                TAB|     |             |
 *	| LEFT_SHIFT | ~  |       SPACE       |RTALT| RIGHT_SHIFT |
 *	|            |  Vx|&                  |     |             |
 *	+------------+----+-------------------+-----+-------------+
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
  ```
  3.3V on Pi ----> 3.3V on Keyboard Pmod or 3.3V on Keyboard FeatherWing.
  GND on Pi -----> GND on Keyboard Pmod or GND on Keyboard FeatherWing.
  SCL(Pi Pin 5/BCM GPIO 3) on Pi ----> SCL on Keyboard Pmod or SCL on Keyboard FeatherWing.
  SDA(Pi Pin 3/BCM GPIO 2) on Pi ----> SDA on Keyboard Pmod or SDA on Keyboard FeatherWing.
  Pi Pin 15/BCM GPIO 22 on Pi ----> INT Pin on Keyboard Pmod or KBD_INT on Keyboard FeatherWing.
  ```

  3. Check pre-requisites with the command `i2cdetect -y 1`(one might need `sudo`). They keyboard should be on the address `0x1F`. If you are using the [Keyboard FeatherWing](https://www.tindie.com/products/arturo182/keyboard-featherwing-qwerty-keyboard-26-lcd/), the Touch sensor on the screen will also show up on `0x4B`.

### Download

Copy the Git repository from above and download it on your Pi, preferably on `/home/pi/` using the below command and change in to the folder.
```bash
git clone https://github.com/wallComputer/bbqX0kbd_driver.git
cd bbqX0kbd_driver/
```

### Installation
- To install the files, use the `installer.sh` file.
  ```bash
  source installer.sh
  ```
- To remove, use the `remover.sh` file.


After following the Newbie or expert guide, reboot your pi and on login, the Keyboard should be working on `/dev/tty1`. To view the above console, Use an LCD such as the one on the [Keyboard FeatherWing](https://www.tindie.com/products/arturo182/keyboard-featherwing-qwerty-keyboard-26-lcd/), or any other you can get to work with your Pi. The easiest way is to either use a Adafruit PiTFT, or use the below example of using [juj's](https://github.com/juj) `frcp-ili9341` project.

- (Optional) [Keyboard FeatherWing](https://www.tindie.com/products/arturo182/keyboard-featherwing-qwerty-keyboard-26-lcd/) has an ILI9341 on it. Using it for the main console can be of great use. The below steps describe how to connect and control this LCD from the Pi.

  0. Hardware connections
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
  1. Install git and cmake.
     ```bash
     sudo apt install git cmake
     ```
  2. Clone [juj's](https://github.com/juj) project, and create a directory `build` inside it, and change in to it.
     ```bash
     git clone https://github.com/juj/fbcp-ili9341.git
     cd fbcp-ili9341
     mkdir build
     cd build/
     ```
  3. Inside the `build` directory, run the below `cmake` command.
     ```bash
     cmake -DILI9341=ON -DSPI_BUS_CLOCK_DIVISOR=40 -DGPIO_TFT_DATA_CONTROL=27 -DSTATISTICS=0 ..
     ```
  4. Run a make command as
     ```bash
     make -j
     ```
  5. The result of the above file will be a `fbcp-ili9341` file. Place the absolute path of this file in the **last line but one** of `/etc/rc.local` as below **(assuming the absolute path is correct)**
     ```bash
     sudo /home/pi/fbcp-ili9341/build/fbcp-ili9341 &
     ```
  6. Place the below text in /boot/config.txt
     ```bash
     hdmi_group=2
     hdmi_mode=87
     hdmi_cvt=320 240 60 1 0 0 0
     hdmi_force_hotplug=1
     ```
  7. From `sudo raspi-config`, under `System Options`, set the `Boot / Auto Login` feature to `Console` or `Console Autologin`. 
  8. Reboot.
  9. On boot up, the LCD should start showing console. It might be useful to run `sudo dpkg-reconfigure console-setup` (can be done over ssh), choose the desired font and size, and place a `sudo /etc/init.d/console-setup.sh reload &` in the `/etc/rc.local` file just as in step 5.

## Customisation

 - For changing the Interrupt Pin on the Raspberry Pi, a few changes are needed.
   1. In the `installer.sh`, change the line `sudo sh -c "echo dtoverlay=i2c-bbqX0kbd,irq_pin=22 >> /boot/config.txt"` to `sudo sh -c "echo dtoverlay=i2c-bbqX0kbd,irq_pin=BCM_PIN >> /boot/config.txt"` where `BCM_PIN` is the BCM Pin on the Pi Header. By default, it's set to GPIO 22, or the Pin 15 of the Raspberry Pi. 
   2. The Settings of the LCD can be changed as shown in [fbcp-ili9341](https://github.com/juj/fbcp-ili9341.git). Notably, the Data_Command Pin can be changed and Clock speed can be increased.
   3. Look at the `bbq10kbd.map` file for alternate key binds. Edit it as per your need and put the new map file in correct folder (as per `installer.sh`) to be loaded correctly in `/etc/default/keyboard`. One can use `sudo loadkeys path/to/bbq10kbd.map` while the driver is running, and press keys to check their new key bindings before placing it in the right location. Once satisfied with your keymap, run `source remover.sh && source installer.sh` to reinstall everything correctly.

## Known Issues

  - The BIGGEST FLAW at the moment is that the driver does not work as intended on Desktop view. The basic keys work, so do Num Lock and Caps Lock features. However, the mapping assigned as per the above keyboard layout does not present itself. Pressing the `LFTALT` key immediately presents the alternate optional menus, which hinders getting any higher and lower keys as per the layout. Two things are suspected, either the Buster Keyboard Layout configuration has some issues (Buster has had issues in Keyboard config for me from `raspi-config`, or else it's as intended behaviour, `Alt` Key pressed on a GUI indicates using other keys for menu options and not for printing characters. 
  - `RTALT`+`~` produces `MUTE` Key. However on Console, it also puts out the `~` key. I have not yet been able to find the cause of this issue. 
  - The entire feature of Volume control is untested as of now. `evtest` shows the `Volume Up`, `Volume Down`, and `MUTE` keys pressed, but as the keyboard driver works the best on console only at the moment, I'm not sure how this function works on a Desktop GUI.

## Future Work
 - ~~Move the fifo reading task to a work queue. This will require changes in a few places, though the core logic should not change.~~ [DONE: using `delayed_work_branch`]
 - Solve the BIGGEST FLAW
 - Make the code compatible with upcoming works of [Solder Party](https://www.solder.party/community/) Keyboards.
 - Add The touch screen sensor to Keyboard Driver itself.
 - Add [Pimoroni trackball](https://shop.pimoroni.com/products/trackball-breakout) to be used as single press mouse to the screen and keyboard.
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