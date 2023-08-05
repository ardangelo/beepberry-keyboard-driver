/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Keyboard Driver for Blackberry Keyboards BBQ10 from arturo182. Software written by wallComputer.
 * bbq10kbd_registers.h: Registers in BBQ10 Keyboard Software.
 */

#ifndef BBQX0KBD_REGISTERS_H_
#define BBQX0KBD_REGISTERS_H_

#include "config.h"


#define BBQX0KBD_I2C_ADDRESS			BBQX0KBD_ASSIGNED_I2C_ADDRESS

#define BBQX0KBD_WRITE_MASK				0x80
#define BBQX0KBD_FIFO_SIZE				31

#define REG_VER                         0x01
#if (BBQX0KBD_TYPE == BBQ10KBD_FEATHERWING)
#define BBQX0KBD_I2C_SW_VERSION			0x04	// Unused for now since the version numbering has reset.
#endif
#if (BBQX0KBD_TYPE == BBQ20KBD_PMOD)
#define BBQX0KBD_I2C_SW_VERSION			0x10
#endif
#define REG_CFG                         0x02
#define REG_CFG_USE_MODS                BIT(7)
#define REG_CFG_REPORT_MODS             BIT(6)
#define REG_CFG_PANIC_INT               BIT(5)
#define REG_CFG_KEY_INT                 BIT(4)
#define REG_CFG_NUMLOCK_INT             BIT(3)
#define REG_CFG_CAPSLOCK_INT            BIT(2)
#define REG_CFG_OVERFLOW_INT            BIT(1)
#define REG_CFG_OVERFLOW_ON             BIT(0)
#define REG_CFG_DEFAULT_SETTING         (REG_CFG_REPORT_MODS | REG_CFG_KEY_INT | REG_CFG_OVERFLOW_ON | REG_CFG_OVERFLOW_INT)
// Configurations Used By Arturo182's Code.
// #define REG_CFG_DEFAULT_SETTING         (REG_CFG_OVERFLOW_ON | REG_CFG_OVERFLOW_INT | REG_CFG_CAPSLOCK_INT | REG_CFG_NUMLOCK_INT | REG_CFG_KEY_INT | REG_CFG_REPORT_MODS )

#define REG_INT                         0x03
#if (BBQX0KBD_TYPE == BBQ20KBD_PMOD)
#define REG_INT_TOUCH					BIT(6)
#endif
#define REG_INT_GPIO                    BIT(5)
#define REG_INT_PANIC                   BIT(4)
#define REG_INT_KEY                     BIT(3)
#define REG_INT_NUMLOCK                 BIT(2)
#define REG_INT_CAPSLOCK                BIT(1)
#define REG_INT_OVERFLOW                BIT(0)
#define REG_INT_RESET_VALUE             0x00

#define REG_KEY                         0x04
#define REG_KEY_NUMLOCK                 BIT(6)
#define REG_KEY_CAPSLOCK                BIT(5)
#define REG_KEY_KEYCOUNT_MASK           0x1F

#define REG_BKL                         0x05

#define REG_DEB                         0x06

#define REG_FRQ                         0x07

#define REG_RST                         0x08
#define REG_FIF                         0x09

#define REG_BK2                         0x0A

#define REG_DIR                         0x0B
#define REG_DIR_INPUT                   1
#define REG_DIR_OUTPUT                  0

#define REG_PUE                         0x0C
#define REG_PUE_ENABLE                  1
#define REG_PUE_DISABLE                 0


#define REG_PUD                         0x0D
#define REG_PUD_PULL_UP                 1
#define REG_PUD_PULL_DOWN               0


#define REG_GIO                         0x0E

#define REG_GIC                         0x0F
#define REG_GIC_INTERRUPT_TRIGGER       1
#define REG_GIC_NO_INTERRUPT_TRIGGER    0

#define REG_GIN                         0x10
#define REG_GIN_RESET_VALUE             0x00

#define BBQ10_BRIGHTNESS_DELTA			16

#if (BBQX0KBD_TYPE == BBQ20KBD_PMOD)
#define REG_CF2							0x14
#define REG_CF2_USB_MOUSE_ON			BIT(2)
#define REG_CF2_USB_KEYB_ON				BIT(1)
#define REG_CF2_TOUCH_INT				BIT(0)
#define REG_CFG2_DEFAULT_SETTING		(REG_CF2_TOUCH_INT)

#define REG_TOX							0x15
#define REG_TOY							0x16

#define REG_ADC 						0x17
#define REG_LED 						0x20
#define REG_LED_R 						0x21
#define REG_LED_G 						0x22
#define REG_LED_B 						0x23
#endif


#endif
