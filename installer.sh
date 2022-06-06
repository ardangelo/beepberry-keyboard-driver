#!/bin/bash

#!/bin/bash

BBQX0KBD_TYPE=${BBQX0KBD_TYPE:-BBQ20KBD_PMOD}
BBQ20KBD_TRACKPAD_USE=${BBQ20KBD_TRACKPAD_USE:-BBQ20KBD_TRACKPAD_AS_MOUSE}
BBQX0KBD_INT=${BBQX0KBD_INT:-BBQX0KBD_NO_INT}
BBQX0KBD_INT_PIN=${BBQX0KBD_INT_PIN:-4}
BBQX0KBD_POLL_PERIOD=${BBQX0KBD_POLL_PERIOD:-40}
BBQX0KBD_ASSIGNED_I2C_ADDRESS=${BBQX0KBD_ASSIGNED_I2C_ADDRESS=BBQX0KBD_DEFAULT_I2C_ADDRESS}
BBQX0KBD_I2C_ADDRESS=0x1F

usage="$(basename "$0") [-h] [--o n]
--BBQX0KBD_TYPE : Keyboard Version You want to build kernel driver for.
		 	Available Options:
				BBQ10KBD_PMOD
				BBQ10KBD_FEATHERWING
				BBQ20KBD_PMOD (Default)
--BBQ20KBD_TRACKPAD_USE : Use BBQ20KBD_PMOD's  trackpad either as a mouse, or as KEY_HOME.
			Availabe Options:
				BBQ20KBD_TRACKPAD_AS_MOUSE (Default)
				BBQ20KBD_TRACKPAD_AS_KEYS
--BBQX0KBD_INT : Build a driver that uses the external Interrupt Pin, or build one without it, polling the Keyboard instead.
			Available Options:
				BBQX0KBD_USE_INT
				BBQX0KBD_NO_INT (Default)
--BBQX0KBD_INT_PIN : Raspberry Pi BCM Pin Number for the Interrupt Pin. Default value is 4. Note that BCM Pin Numbers are different from Pi's header Pin Numbers.
--BBQX0KBD_POLL_PERIOD : Polling Rate for the Keyboard in milliseconds. Value between 10 to 1000. Defualt is 40.
--BBQX0KBD_ASSIGNED_I2C_ADDRESS : I2C Address if changed. Default Value is 0x1F.
Examples:

>./installer.sh --BBQX0KBD_TYPE BBQ10KBD_FEATHERWING --BBQX0KBD_INT BBQX0KBD_NO_INT --BBQX0KBD_POLL_PERIOD 25
This creates the kernel driver for Keyboard Featherwing. It does not use the Interrupt Pin, instead polls the keyboard every 25ms.

>./installer.sh --BBQX0KBD_TYPE BBQ10KBD_PMOD --BBQX0KBD_INT BBQX0KBD_USE_INT --BBQX0KBD_INT_PIN 23
This creates the kernel driver for BBQ10KBD_PMOD. It does use the Interrupt Pin, namely, BCM Pin GPIO 23 (Pin 16 on the header).

>./installer.sh --BBQX0KBD_TYPE BBQ20KBD_PMOD --BBQX0KBD_INT BBQX0KBD_NO_INT --BBQX0KBD_ASSIGNED_I2C_ADDRESS 0x45
This creates the kernel driver for BBQ20KBD_PMOD. The Driver assumes the Keyboard is available on I2C Address 0x45, as well as polls it at default poll rate of 40ms.

>./installer.sh --BBQX0KBD_TYPE BBQ20KBD_PMOD --BBQ20KBD_TRACKPAD_USE BBQ20KBD_TRACKPAD_AS_KEYS
This creates the kernel driver for BBQ20KBD_PMOD and uses the trackpad as KEY_UP, KEY_DOWN, KEY_LEFT, and KEY_RIGHT.
"




while [ $# -gt 0 ]; do

   if [ "$1" == "-h" -o "$1" == "--h" -o "$1" == "-H" -o "$1" == "--H" ]; then
      echo "$usage"
      exit 1;
   fi

   if [[ $1 == *"--"* ]]; then
        param="${1/--/}"
        declare $param="$2"
        # echo $1 $2 // Optional to see the parameter:value result
   fi

  shift
done

if [ "$BBQX0KBD_TYPE" != "BBQ10KBD_PMOD" ] && [ "$BBQX0KBD_TYPE" != "BBQ10KBD_FEATHERWING" ] && [ "$BBQX0KBD_TYPE" != "BBQ20KBD_PMOD" ]; then
	echo "Could not recognise BBQX0KBD_TYPE, possible Spelling Error? Check with help."
	echo "$usage"
	exit 1
fi

if [ "$BBQX0KBD_INT" != "BBQX0KBD_NO_INT" ] && [ "$BBQX0KBD_INT" != "BBQX0KBD_USE_INT" ]; then
	echo "Could not recognise BBQX0KBD_INT, possible Spelling Error? Check with help."
	echo "$usage"
	exit 1
fi

if [ "$BBQX0KBD_TYPE" == "BBQ20KBD_PMOD" ] &&  [ "$BBQ20KBD_TRACKPAD_USE" != "BBQ20KBD_TRACKPAD_AS_MOUSE" ] && [ "$BBQ20KBD_TRACKPAD_USE" != "BBQ20KBD_TRACKPAD_AS_KEYS" ]; then
	echo "Could not recognise BBQX0KBD_TRACKPAD_USE, possible Spelling Error? Check with help."
	echo "$usage"
	exit 1
fi


echo "Keyboard Type: "$BBQX0KBD_TYPE
if [ "$BBQX0KBD_TYPE" == "BBQ20KBD_PMOD" ]; then
	echo "BBQ20KBD TrackPad Use:" $BBQ20KBD_TRACKPAD_USE
fi
echo "Keyboard Use Interrupt: "$BBQX0KBD_INT
if [ "$BBQX0KBD_INT" == "BBQX0KBD_USE_INT" ]; then
	echo "Keyboard Interrupt Pin: "$BBQX0KBD_INT_PIN
else
	echo "Keyboard Polling Period: "$BBQX0KBD_POLL_PERIOD"ms"
fi
echo "Keyboard I2C Address:" $BBQX0KBD_ASSIGNED_I2C_ADDRESS


sed -i  "s/BBQX0KBD_TYPE BBQ.*/BBQX0KBD_TYPE ${BBQX0KBD_TYPE}/g" source/mod_src/config.h
sed -i  "s/BBQ20KBD_TRACKPAD_USE BBQ20KBD_TRACKPAD_AS.*/BBQ20KBD_TRACKPAD_USE ${BBQ20KBD_TRACKPAD_USE}/g" source/mod_src/config.h
sed -i  "s/BBQX0KBD_INT BBQ.*/BBQX0KBD_INT ${BBQX0KBD_INT}/g" source/mod_src/config.h
sed -i  "s/BBQX0KBD_INT_PIN .*/BBQX0KBD_INT_PIN ${BBQX0KBD_INT_PIN}/g" source/mod_src/config.h
sed -i  "s/BBQX0KBD_POLL_PERIOD .*/BBQX0KBD_POLL_PERIOD ${BBQX0KBD_POLL_PERIOD}/g" source/mod_src/config.h
sed -i  "s/BBQX0KBD_ASSIGNED_I2C_ADDRESS .*/BBQX0KBD_ASSIGNED_I2C_ADDRESS ${BBQX0KBD_ASSIGNED_I2C_ADDRESS}/g" source/mod_src/config.h


if [ "$BBQX0KBD_INT" != "BBQX0KBD_USE_INT" ]; then
	cp source/dts_src/i2c-bbqX0kbd-polling.dts source/dts_src/i2c-bbqX0kbd.dts
else
	cp source/dts_src/i2c-bbqX0kbd-int.dts source/dts_src/i2c-bbqX0kbd.dts
fi

if [ "$BBQX0KBD_ASSIGNED_I2C_ADDRESS" != "BBQX0KBD_DEFAULT_I2C_ADDRESS" ]; then
	BBQX0KBD_I2C_ADDRESS=$BBQX0KBD_ASSIGNED_I2C_ADDRESS
	BBQX0KBD_I2C_ADDRESS_ID=$(echo $BBQX0KBD_I2C_ADDRESS | cut -c3-4)
	if [ $(echo $BBQX0KBD_I2C_ADDRESS_ID | cut -c1-1) == "0" ]; then
		BBQX0KBD_I2C_ADDRESS_ID=$(echo $BBQX0KBD_I2C_ADDRESS_ID | cut -c2-2)
	fi
	sed -i "s/bbqX0kbd: bbqX0kbd@.*/bbqX0kbd: bbqX0kbd@${BBQX0KBD_I2C_ADDRESS_ID} {/g" source/dts_src/i2c-bbqX0kbd.dts
	sed -i "s/reg = .*/reg = <${BBQX0KBD_I2C_ADDRESS}>;/g" source/dts_src/i2c-bbqX0kbd.dts
fi

if make && make dtbo ; then

	if [ "$BBQX0KBD_INT" == "BBQX0KBD_USE_INT" ]; then
		sudo sh -c "echo dtoverlay=i2c-bbqX0kbd,irq_pin=$BBQX0KBD_INT_PIN >> /boot/config.txt"
	else
	    sudo sh -c "echo dtoverlay=i2c-bbqX0kbd >> /boot/config.txt"
	fi
	sudo cp i2c-bbqX0kbd.dtbo /boot/overlays/

	sudo sh -c "echo bbqX0kbd >> /etc/modules"
	sudo cp bbqX0kbd.ko /lib/modules/$(uname -r)/kernel/drivers/i2c/bbqX0kbd.ko

	sudo sh -c "echo KMAP=\"/usr/local/share/kbd/keymaps/bbqX0kbd.map\" >> /etc/default/keyboard"
	sudo mkdir -p /usr/local/share/kbd/keymaps/
	sudo cp source/mod_src/bbqX0kbd.map /usr/local/share/kbd/keymaps/bbqX0kbd.map

	sudo depmod

	echo -e "\nmake succeeded, please reboot to get the keyboard working."

else
	echo "MAKE DID NOT SUCCEED!!!\n"
fi


