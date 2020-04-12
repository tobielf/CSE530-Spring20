/**
 * @file gpio_config.c
 * @brief pin configuration of hcsr04 device on Intel Galileo board
 * @author Xiangyu Guo
 */
#include <linux/gpio.h>

#include "gpio_config.h"

typedef struct multi_plexing {
        int logic;                      /**< Linux Logic Pin */
        int dir;                        /**< Direction Shifter */
        int mux1;                       /**< Pin Mux 1 */
        int mux2;                       /**< Pin Mux 2 */
        int mux;                        /**< Mux level */
} multi_plexing_t;

/**< Mapping from shield pins to Logic Pin */
static multi_plexing_t shield_pins[] = {
        {
                .logic = 11,
                .dir   = 32,
                .mux1  = -1,
                .mux2  = -1,
                .mux   = GPIOF_OUT_INIT_LOW
        }, {
                .logic = 12,
                .dir   = 28,
                .mux1  = 45,
                .mux2  = -1,
                .mux   = GPIOF_OUT_INIT_LOW
        }, {
                .logic = 61,
                .dir   = -1,
                .mux1  = 77,
                .mux2  = -1,
                .mux   = GPIOF_OUT_INIT_LOW
        }, {
                .logic = 62,
                .dir   = -1,
                .mux1  = 76,
                .mux2  = 64,
                .mux   = GPIOF_OUT_INIT_LOW
        }, {
                .logic = 6,
                .dir   = 36,
                .mux1  = -1,
                .mux2  = -1,
                .mux   = GPIOF_OUT_INIT_LOW
        }, {
                .logic = 0,
                .dir   = 18,
                .mux1  = 66,
                .mux2  = -1,
                .mux   = GPIOF_OUT_INIT_LOW
        }, {
                .logic = 1,
                .dir   = 20,
                .mux1  = 68,
                .mux2  = -1,
                .mux   = GPIOF_OUT_INIT_LOW
        }, {
                .logic = 38,
                .dir   = -1,
                .mux1  = -1,
                .mux2  = -1,
                .mux   = GPIOF_OUT_INIT_LOW
        }, {
                .logic = 40,
                .dir   = -1,
                .mux1  = -1,
                .mux2  = -1,
                .mux   = GPIOF_OUT_INIT_LOW
        }, {
                .logic = 4,
                .dir   = 22,
                .mux1  = 70,
                .mux2  = -1,
                .mux   = GPIOF_OUT_INIT_LOW
        }, {
                .logic = 10,
                .dir   = 26,
                .mux1  = 74,
                .mux2  = -1,
                .mux   = GPIOF_OUT_INIT_LOW
        }, {
                .logic = 5,
                .dir   = 24,
                .mux1  = 44,
                .mux2  = 72,
                .mux   = GPIOF_OUT_INIT_LOW
        }, {
                .logic = 15,
                .dir   = 42,
                .mux1  = -1,
                .mux2  = -1,
                .mux   = GPIOF_OUT_INIT_LOW
        }, {
                .logic = 7,
                .dir   = 30,
                .mux1  = 46,
                .mux2  = -1,
                .mux   = GPIOF_OUT_INIT_LOW
        }, {
                .logic = 48,
                .dir   = -1,
                .mux1  = -1,
                .mux2  = -1,
                .mux   = GPIOF_OUT_INIT_LOW
        }, {
                .logic = 50,
                .dir   = -1,
                .mux1  = -1,
                .mux2  = -1,
                .mux   = GPIOF_OUT_INIT_LOW
        }, {
                .logic = 52,
                .dir   = -1,
                .mux1  = -1,
                .mux2  = -1,
                .mux   = GPIOF_OUT_INIT_LOW
        }, {
                .logic = 54,
                .dir   = -1,
                .mux1  = -1,
                .mux2  = -1,
                .mux   = GPIOF_OUT_INIT_LOW
        }, {
                .logic = 56,
                .dir   = -1,
                .mux1  = 60,
                .mux2  = 78,
                .mux   = GPIOF_OUT_INIT_HIGH
        }, {
                .logic = 58,
                .dir   = -1,
                .mux1  = 60,
                .mux2  = 79,
                .mux   = GPIOF_OUT_INIT_HIGH
        }
};

/* @note Shield pin 11 and 13 are reserved for SPI purpose */
/**< Flag to store the pin usage status */
static unsigned int pin_usage = 0b01 << 11 | 0b01 << 13;

/**< Flag to represent the pin support interrupt both */
static unsigned int interrupt_both = 0b01 << 2  |
                                     0b01 << 3  |
                                     0b01 << 4  |
                                     0b01 << 5  |
                                     0b01 << 6  |
                                     0b01 << 9  |
                                     // 0b01 << 11 |
                                     // 0b01 << 13 |
                                     0b01 << 14 |
                                     0b01 << 15 |
                                     0b01 << 16 |
                                     0b01 << 17 |
                                     0b01 << 18 |
                                     0b01 << 19;

/**
 * @brief setting up the pwm pin value
 * @param flag, high or low
 * @param pin, GPIO pin number
 */
static void quark_gpio_set_pwm_value(int flag, int pin) {
        if (flag == GPIOF_OUT_INIT_LOW) {
                gpio_set_value_cansleep(pin, 0);
        } else {
                gpio_set_value_cansleep(pin, 1);
        }
}

int quark_gpio_init_pin(int pin, int dir) {
        int ret;
        int flag = GPIOF_OUT_INIT_LOW;
        multi_plexing_t *mp = &shield_pins[pin];
        printk(KERN_INFO "shield: %d, logic: %d, dir: %d, mux1: %d, mux2: %d, mux: %d\n",
                pin, mp->logic, mp->dir, mp->mux1, mp->mux2, mp->mux);

        if (dir == INPUT) {
            flag = GPIOF_IN;
        }
        // logic
        ret = gpio_request_one(mp->logic, flag, NULL);
        if (ret) {
                printk(KERN_ALERT "Config GPIO %d failed!\n", mp->logic);
                return -EBUSY;
        }

        if (dir == INPUT) {
            flag = GPIOF_OUT_INIT_HIGH;
        }
        // dir
        if (mp->dir != -1) {
                ret = gpio_request_one(mp->dir, flag, NULL);
        }
        // mux1
        if (mp->mux1 != -1 && mp->mux1 < 64) {
                ret = gpio_request_one(mp->mux1, mp->mux, NULL);
        } else if (mp->mux1 != -1) {
                ret = gpio_request(mp->mux1, NULL);
                quark_gpio_set_pwm_value(mp->mux, mp->mux1);
        }
        // mux2
        if (mp->mux2 != -1 && mp->mux2 < 64) {
                ret = gpio_request_one(mp->mux2, mp->mux, NULL);
        } else if (mp->mux2 != -1) {
                ret = gpio_request(mp->mux2, NULL);
                quark_gpio_set_pwm_value(mp->mux, mp->mux2);
        }
        pin_usage |= (1 << pin);
        return 0;
}

int quark_gpio_fini_pin(int pin) {
        multi_plexing_t *mp = &shield_pins[pin];
        gpio_free(mp->logic);
        if (mp->dir != -1)
                gpio_free(mp->dir);
        if (mp->mux1 != - 1)
                gpio_free(mp->mux1);
        if (mp->mux2 != -1)
                gpio_free(mp->mux2);
        pin_usage &= ~(1 << pin);
        return 0;
}

int quark_gpio_config_validate_pin(int pin) {
        if (0 > pin || 20 < pin) {
                printk(KERN_ALERT "Pin(s) not in the correct range!\n");
                return -EINVAL;
        }
        if (pin_usage & (1 << pin)) {
                printk(KERN_ALERT "Pin(s) already in use!\n");
                return -EINVAL;
        }
        return 0;
}

int quark_gpio_config_validate_echo(int pin) {
        if (~interrupt_both & (1 << pin)) {
                printk(KERN_ALERT "This pin can't be echo pin!\n");
                return -EINVAL;
        }
        return 0;
}

int quark_gpio_shield_to_gpio(int pin) {
        if (pin < 0 || pin > ARRAY_SIZE(shield_pins))
                return -EINVAL;

        return shield_pins[pin].logic;
}
