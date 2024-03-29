/**
 * @file hcsr_config.h
 * @brief pin configuration of hcsr04 device on Intel Galileo board
 * @author Xiangyu Guo
 */
#ifndef __HCSR_CONFIG_H__
#define __HCSR_CONFIG_H__

#include "common.h"

enum direction {
    INPUT,
    OUTPUT,
};

/** 
 * @brief validate the pin.
 * @param pin, a valid pin.
 * @return 0 on success, otherwise errno.
 */
int hcsr04_config_validate_pin(int);

/** 
 * @brief validate the pin available for echo.
 * @param pin, a valid pin.
 * @return 0 on success, otherwise errno.
 */
int hcsr04_config_validate_echo(int);

/** 
 * @brief validate the pin.
 * @param pins, a valid pointer to the pins structure.
 * @return 0 on success, otherwise errno.
 */
int hcsr04_config_validate(pins_setting_t *);

/** 
 * @brief setup the pins.
 * @param pins, a valid pointer to the pins structure.
 * @return 0 on success, otherwise errno.
 */
int hcsr04_config_init(pins_setting_t *);

/** 
 * @brief teardown the pins.
 * @param pins, a valid pointer to the pins structure.
 * @return 0 on success, otherwise errno.
 */
int hcsr04_config_fini(pins_setting_t *);

/** 
 * @brief setting up pin.
 * @param pin, shield pin number.
 * @param dir, direction of the shield pin.
 * @return 0 on success, otherwise errno.
 */
int hcsr04_init_pins(int, int);

/** 
 * @brief free up all pins.
 * @param pin, shield pin number.
 * @return 0 on success, otherwise errno.
 */
int hcsr04_fini_pins(int);

/** 
 * @brief convert the pin number from shield pin # to linux pin #.
 * @param pin, shield pin number
 * @return positive on valid linux pin #, otherwise errno.
 */
int hcsr04_shield_to_gpio(int pin);

#endif