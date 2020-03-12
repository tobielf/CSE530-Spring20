/**
 * @file hcsr_config.c
 * @brief pin configuration of hcsr04 device on Intel Galileo board
 * @author Xiangyu Guo
 */
#include <linux/gpio.h>

#include "hcsr_config.h"

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

/**< Flag to store the pin usage status */
static unsigned int pin_usage = 0;

/**< Flag to represent the pin support interrupt both */
static unsigned int interrupt_both = 0b01 << 4  |
                                     0b01 << 5  |
                                     0b01 << 6  |
                                     0b01 << 9  |
                                     0b01 << 11 |
                                     0b01 << 13;

/**
 * @brief setting up the pwm pin value
 * @param flag, high or low
 * @param pin, GPIO pin number
 */
static void hcsr04_set_pwm_value(int flag, int pin) {
        if (flag == GPIOF_OUT_INIT_LOW) {
                gpio_set_value_cansleep(pin, 0);
        } else {
                gpio_set_value_cansleep(pin, 1);
        }
}

int hcsr04_init_echo(int pin) {
        int ret;
        multi_plexing_t *mp = &shield_pins[pin];
        printk(KERN_INFO "shield: %d, logic: %d, dir: %d, mux1: %d, mux2: %d, mux: %d\n",
                pin, mp->logic, mp->dir, mp->mux1, mp->mux2, mp->mux);

        // logic
        ret = gpio_request_one(mp->logic, GPIOF_IN, NULL);
        if (ret) {
                printk(KERN_ALERT "Config GPIO %d failed!\n", mp->logic);
                return -EBUSY;
        }
        // dir
        if (mp->dir != -1) {
                ret = gpio_request_one(mp->dir, GPIOF_OUT_INIT_HIGH, NULL);
        }
        // mux1
        if (mp->mux1 != -1 && mp->mux1 < 64) {
                ret = gpio_request_one(mp->mux1, mp->mux, NULL);
        } else if (mp->mux1 != -1) {
                ret = gpio_request(mp->mux1, NULL);
                hcsr04_set_pwm_value(mp->mux, mp->mux1);
        }
        // mux2
        if (mp->mux2 != -1 && mp->mux2 < 64) {
                ret = gpio_request_one(mp->mux2, mp->mux, NULL);
        } else if (mp->mux2 != -1) {
                ret = gpio_request(mp->mux2, NULL);
                hcsr04_set_pwm_value(mp->mux, mp->mux2);
        }
        pin_usage |= (1 << pin);
        return 0;
}

int hcsr04_init_trigger(int pin) {
        int ret;
        multi_plexing_t *mp = &shield_pins[pin];
        printk(KERN_INFO "shield: %d, logic: %d, dir: %d, mux1: %d, mux2: %d, mux: %d\n",
                pin, mp->logic, mp->dir, mp->mux1, mp->mux2, mp->mux);
        // logic
        ret = gpio_request_one(mp->logic, GPIOF_OUT_INIT_LOW, NULL);
        if (ret) {
                printk(KERN_ALERT "Config GPIO %d failed!\n", mp->logic);
                return -EBUSY;
        }
        // dir
        if (mp->dir != -1) {
                ret = gpio_request_one(mp->dir, GPIOF_OUT_INIT_LOW, NULL);
        }
        // mux1
        if (mp->mux1 != -1 && mp->mux1 < 64) {
                ret = gpio_request_one(mp->mux1, mp->mux, NULL);
        } else if (mp->mux1 != -1) {
                ret = gpio_request(mp->mux1, NULL);
                hcsr04_set_pwm_value(mp->mux, mp->mux1);
        }
        // mux2
        if (mp->mux2 != -1 && mp->mux2 < 64) {
                ret = gpio_request_one(mp->mux2, mp->mux, NULL);
        } else if (mp->mux2 != -1) {
                ret = gpio_request(mp->mux2, NULL);
                hcsr04_set_pwm_value(mp->mux, mp->mux2);
        }
        pin_usage |= (1 << pin);
        return 0;
}

int hcsr04_fini_pins(int pin) {
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

int hcsr04_config_validate_pin(int pin) {
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

int hcsr04_config_validate_echo(int pin) {
        if (~interrupt_both & (1 << pin)) {
                printk(KERN_ALERT "This pin can't be echo pin!\n");
                return -EINVAL;
        }
        return 0;
}

int hcsr04_config_validate(pins_setting_t *pins) {

        if (hcsr04_config_validate_pin(pins->trigger_pin) ||
            hcsr04_config_validate_pin(pins->echo_pin)) {
                return -EINVAL;
        }

        if (hcsr04_config_validate_echo(pins->echo_pin))
                return -EINVAL;

        return 0;
}

int hcsr04_config_init(pins_setting_t *pins) {
        if (hcsr04_config_validate(pins))
                return -EINVAL;

        if (hcsr04_init_trigger(pins->trigger_pin) || 
                hcsr04_init_echo(pins->echo_pin))
                return -EBUSY;

        pin_usage |= (1 << pins->trigger_pin) | (1 << pins->echo_pin);
        return 0;
}

int hcsr04_config_fini(pins_setting_t *pins) {
        if (pins->trigger_pin == -1 || pins->echo_pin == -1)
                return 0;

        hcsr04_fini_pins(pins->trigger_pin);
        hcsr04_fini_pins(pins->echo_pin);
        pin_usage &= ~((1 << pins->trigger_pin) | (1 << pins->echo_pin));
        return 0;
}

int hcsr04_shield_to_gpio(int pin) {
        if (pin < 0 || pin > ARRAY_SIZE(shield_pins))
                return -EINVAL;

        return shield_pins[pin].logic;
}
