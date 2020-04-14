/**
 * @file heartbeat-core.c
 * @brief High level driver of Heartbeat driver
 * 1. Using SPI to control the MAX7219 LED Matrix
 * 2. Using GPIO to control HCSR04 ultrasonic sensor
 * 3. Using Netlink to communicate with userspace program
 *
 * @author Xiangyu Guo
 * @reference /kernel/drivers/spi/spidev.c
 * @reference https://github.com/a-zaki/genl_ex
 */
#include <linux/module.h>
#include <linux/gpio.h>

#include <linux/delay.h>

#include <net/genetlink.h>

#include "common.h"
#include "gpio_config.h"
#include "max7219.h"

#define GENL_HB_HELLO_INTERVAL        5000      /**< Timeout to send reply */

static struct timer_list timer;                 /**< Timer for the delay */

static genl_hb_pins_t s_pins = {-1, -1, -1};    /**< Chip select, trigger, and echo pin */

/**
 * @brief heartbeat pin configure [input] message.
 * @param skb, socket buffer.
 * @param info, generic netlink info.
 * @param return 0 on success, otherwise failed.
 */
static int genl_hb_rx_pin_config_msg(struct sk_buff* skb, struct genl_info* info);

/**
 * @brief heartbeat measurement [input] message.
 * @param skb, socket buffer.
 * @param info, generic netlink info.
 * @param return 0 on success, otherwise failed.
 */
static int genl_hb_rx_measure_msg(struct sk_buff* skb, struct genl_info* info);

/**
 * @brief heartbeat pattern display [input] message.
 * @param skb, socket buffer.
 * @param info, generic netlink info.
 * @param return 0 on success, otherwise failed.
 */
static int genl_hb_rx_display_msg(struct sk_buff* skb, struct genl_info* info);

/**
 * @brief heartbeat distance [output] message.
 * @param data, the data pass through the callback.
 */
static void genl_hb_tx_distance_msg(unsigned long data);

/** Supported generic netlink operations */
static const struct genl_ops genl_hb_ops[] = {
        {
                .cmd = GENL_HB_CONFIG_MSG,
                .policy = genl_hb_policy,
                .doit = genl_hb_rx_pin_config_msg,
                .dumpit = NULL,
        }, {
                .cmd = GENL_HB_MEASURE_MSG,
                .policy = genl_hb_policy,
                .doit = genl_hb_rx_measure_msg,
                .dumpit = NULL,
        }, {
                .cmd = GENL_HB_DISPLAY_MSG,
                .policy = genl_hb_policy,
                .doit = genl_hb_rx_display_msg,
                .dumpit = NULL,
        },
};

/** Define a protocol family */
static struct genl_family genl_hb_family = {
        .name = GENL_HB_FAMILY_NAME,
        .version = 1,
        .maxattr = GENL_HB_ATTR_MAX,
        .netnsok = false,
        .module = THIS_MODULE,
        .ops = genl_hb_ops,
        .n_ops = ARRAY_SIZE(genl_hb_ops),
};

static int genl_hb_rx_pin_config_msg(struct sk_buff* skb, struct genl_info* info)
{
        genl_hb_pins_t pins;
        if (!info->attrs[GENL_HB_ATTR_MSG]) {
                printk(KERN_ERR "empty message from %d!!\n", info->snd_portid);
                printk(KERN_ERR "%p\n", info->attrs[GENL_HB_ATTR_MSG]);
                return -EINVAL;
        }

        printk(KERN_INFO "length:%d\n", nla_len(info->attrs[GENL_HB_ATTR_MSG]));

        memcpy(&pins, nla_data(info->attrs[GENL_HB_ATTR_MSG]), sizeof(genl_hb_pins_t));
        printk(KERN_NOTICE "%u says chip_select:%d trigger:%d echo:%d\n", info->snd_portid,
                                pins.chip_select, pins.trigger, pins.echo);

        // Invoke GPIO module to configure pins.
        // Reverse previous settings.[ToDo]Check return values.
        if (s_pins.chip_select != -1) {
                quark_gpio_fini_pin(s_pins.chip_select);
                s_pins.chip_select = -1;
        }
        if (s_pins.trigger != -1) {
                quark_gpio_fini_pin(s_pins.trigger);
                s_pins.trigger = -1;
        }
        if (s_pins.echo != -1) {
                quark_gpio_fini_pin(s_pins.echo);
                s_pins.echo = -1;
        }
        
        // Validate pins, not occupied.
        if (pins.chip_select == pins.trigger ||
            pins.chip_select == pins.echo    ||
            pins.trigger     == pins.echo    ||
            quark_gpio_config_validate_pin(pins.chip_select) ||
            quark_gpio_config_validate_pin(pins.trigger) ||
            quark_gpio_config_validate_pin(pins.echo) ||
            quark_gpio_config_validate_echo(pins.echo)) {
                return -EINVAL;
        }

        // Setup new pins. [ToDo]Check return values.
        quark_gpio_init_pin(pins.chip_select, OUTPUT);
        quark_gpio_init_pin(pins.trigger, OUTPUT);
        quark_gpio_init_pin(pins.echo, INPUT);
        s_pins.chip_select = pins.chip_select;
        s_pins.trigger = pins.trigger;
        s_pins.echo = pins.echo;
        return 0;
}

static int genl_hb_rx_measure_msg(struct sk_buff* skb, struct genl_info* info) {
        
        if (!info->attrs[GENL_HB_ATTR_MSG]) {
                printk(KERN_ERR "empty message from %d!!\n", info->snd_portid);
                printk(KERN_ERR "%p\n", info->attrs[GENL_HB_ATTR_MSG]);
                return -EINVAL;
        }

        printk(KERN_INFO "length:%d\n", nla_len(info->attrs[GENL_HB_ATTR_MSG]));

        printk(KERN_NOTICE "Going to do the measurement.\n");

        timer.data = (unsigned long)info->snd_portid;
        timer.function = genl_hb_tx_distance_msg;
        timer.expires = jiffies + msecs_to_jiffies(GENL_HB_HELLO_INTERVAL);
        add_timer(&timer);

        // [ToDo] Invoke HCSR module to measure the distance.

        return 0;
}

static int genl_hb_rx_display_msg(struct sk_buff* skb, struct genl_info* info) {
        int cs_gpio;
        uint8_t i;
        uint8_t tx[2];
        genl_hb_pattern_t pattern;

        if (!info->attrs[GENL_HB_ATTR_MSG]) {
                printk(KERN_ERR "empty message from %d!!\n", info->snd_portid);
                printk(KERN_ERR "%p\n", info->attrs[GENL_HB_ATTR_MSG]);
                return -EINVAL;
        }

        printk(KERN_INFO "length:%d\n", nla_len(info->attrs[GENL_HB_ATTR_MSG]));

        memcpy(&pattern, nla_data(info->attrs[GENL_HB_ATTR_MSG]), sizeof(genl_hb_pattern_t));

        printk(KERN_NOTICE "Going to display the pattern.\n");
        // [ToDo] Check chip_select initialized.
        cs_gpio = quark_gpio_shield_to_gpio(s_pins.chip_select);

        for (i = 0; i < 8; i++) {
                tx[0] = pattern.led[i];
                tx[1] = i + 1;
                gpio_set_value_cansleep(cs_gpio, 0);
                barrier();
                max7219_send_msg(tx, 2);
                barrier();
                gpio_set_value_cansleep(cs_gpio, 1);
                mdelay(10);
        }

        return 0;
}

static void genl_hb_tx_distance_msg(unsigned long data) {   
        void *hdr;
        u32 portid = (u32)data;
        int res, flags = GFP_ATOMIC;
        char msg[GENL_HB_ATTR_MSG_MAX];
        struct sk_buff* skb = genlmsg_new(NLMSG_DEFAULT_SIZE, flags);

        if (!skb) {
                printk(KERN_ERR "%d: OOM!!", __LINE__);
                return;
        }

        hdr = genlmsg_put(skb, 0, 0, &genl_hb_family, flags, GENL_HB_MEASURE_MSG);
        if (!hdr) {
                printk(KERN_ERR "%d: Unknown err !", __LINE__);
                goto nlmsg_fail;
        }

        snprintf(msg, GENL_HB_ATTR_MSG_MAX, "Hello world\n");

        res = nla_put_string(skb, GENL_HB_ATTR_MSG, msg);
        if (res) {
                printk(KERN_ERR "%d: err %d ", __LINE__, res);
                goto nlmsg_fail;
        }

        genlmsg_end(skb, hdr);

        printk(KERN_INFO "I am going to reply the distance!\n");
        genlmsg_unicast(&init_net, skb, portid);
        return;

nlmsg_fail:
        genlmsg_cancel(skb, hdr);
        nlmsg_free(skb);
        return;
}

static int __init heartbeat_init(void) {
        int ret;
        init_timer(&timer);

        // Initialize MAX7219 LED Driver.
        ret = max7219_device_init();
        if (ret)
                return -EINVAL;

        // Register Netlink family, so the driver is ready to work.
        ret = genl_register_family(&genl_hb_family);
        if (ret)
                return -EINVAL;

        return 0;
}

static void heartbeat_exit(void) {
        // Unregister Netlink family.
        genl_unregister_family(&genl_hb_family);

        // Remove MAX7219 Driver.
        max7219_device_exit();

        if (s_pins.chip_select != -1) {
                quark_gpio_fini_pin(s_pins.chip_select);
                s_pins.chip_select = -1;
        }
        if (s_pins.trigger != -1) {
                quark_gpio_fini_pin(s_pins.trigger);
                s_pins.trigger = -1;
        }
        if (s_pins.echo != -1) {
                quark_gpio_fini_pin(s_pins.echo);
                s_pins.echo = -1;
        }

        del_timer(&timer);
}

module_init(heartbeat_init);
module_exit(heartbeat_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xiangyu Guo");
MODULE_DESCRIPTION("Assignment 3");