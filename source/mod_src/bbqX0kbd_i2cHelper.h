/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Keyboard Driver for Blackberry Keyboards BBQ10 from arturo182. Software written by wallComputer.
 * bbqX0kbd_i2cHelper.h: I2C Functions H file.
 */
#ifndef BBQX0KBD_I2CHELPER_H_
#define BBQX0KBD_I2CHELPER_H_

#include <linux/i2c.h>

#include "config.h"
#include "bbqX0kbd_registers.h"
#include "debug_levels.h"

extern int bbqX0kbd_write(struct i2c_client *i2c_client, uint8_t deviceAddress, uint8_t registerAddress, const uint8_t *buffer, uint8_t bufferSize);
extern int bbqX0kbd_read(struct i2c_client *i2c_client, uint8_t deviceAddress, uint8_t registerAddress, uint8_t *buffer, uint8_t bufferSize);
#endif
