/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Keyboard Driver for Blackberry Keyboards BBQ10 from arturo182. Software written by wallComputer.
 * bbq10kbd_registers.h: Registers in BBQ10 Keyboard Software.
 */

#ifndef BBQ10KBD_REGISTERS_H_
#define BBQ10KBD_REGISTERS_H_


#define BBQ10_I2C_ADDRESS               0x1F

#define BBQ10_WRITE_MASK                0x80

#define REG_VER                         0x01
#define BBQ10_I2C_SW_VERSION            0x04

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
#define KEY_IDLE_STATE					0
#define KEY_PRESSED_STATE               1
#define KEY_PRESSED_AND_HELD_STATE      2
#define KEY_RELEASED_STATE              3

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

#define BBQ10_BRIGHTNESS_DELTA       16


#endif
