/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Keyboard Driver for Blackberry Keyboards BBQ10 from arturo182. Software written by wallComputer.
 * config.h: File to store driver configurations.
 */

#ifndef CONFIG_H_
#define CONFIG_H_

#define BBQX0KBD_DEFAULT_I2C_ADDRESS	0x1F
#define BBQX0KBD_ASSIGNED_I2C_ADDRESS BBQX0KBD_DEFAULT_I2C_ADDRESS

#define BBQ10KBD_PMOD				0
#define BBQ10KBD_FEATHERWING		1
#define BBQ20KBD_PMOD				2
#define BBQX0KBD_TYPE BBQ20KBD_PMOD

#if (BBQX0KBD_TYPE == BBQ20KBD_PMOD)
#define BBQ20KBD_TRACKPAD_AS_MOUSE		0
#define BBQ20KBD_TRACKPAD_AS_KEYS		1
#define BBQ20KBD_TRACKPAD_USE BBQ20KBD_TRACKPAD_AS_KEYS
#endif

#define BBQX0KBD_USE_INT			0
#define BBQX0KBD_NO_INT				1
#define BBQX0KBD_INT BBQX0KBD_USE_INT

#if (BBQX0KBD_INT == BBQX0KBD_NO_INT)
#define BBQX0KBD_POLL_PERIOD 40
#else
#define BBQX0KBD_INT_PIN 4
#endif


#endif
