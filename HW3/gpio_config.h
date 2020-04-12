/**
 * @file gpio_config.h
 * @brief pin configuration of quark_gpio device on Intel Galileo board
 * @author Xiangyu Guo
 */
#ifndef __HCSR_CONFIG_H__
#define __HCSR_CONFIG_H__

enum direction {
    INPUT,
    OUTPUT,
};

/** 
 * @brief validate the pin.
 * @param pin, a valid pin.
 * @return 0 on success, otherwise errno.
 */
int quark_gpio_config_validate_pin(int);

/** 
 * @brief validate the pin available for echo.
 * @param pin, a valid pin.
 * @return 0 on success, otherwise errno.
 */
int quark_gpio_config_validate_echo(int);

/** 
 * @brief setting up pin.
 * @param pin, shield pin number.
 * @param dir, direction of the shield pin.
 * @return 0 on success, otherwise errno.
 */
int quark_gpio_init_pin(int, int);

/** 
 * @brief free up all pins.
 * @param pin, shield pin number.
 * @return 0 on success, otherwise errno.
 */
int quark_gpio_fini_pin(int);

/** 
 * @brief convert the pin number from shield pin # to linux pin #.
 * @param pin, shield pin number
 * @return positive on valid linux pin #, otherwise errno.
 */
int quark_gpio_shield_to_gpio(int pin);

#endif