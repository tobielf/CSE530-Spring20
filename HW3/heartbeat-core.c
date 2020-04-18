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

#include <net/genetlink.h>

#include "common.h"
#include "gpio_config.h"
#include "max7219.h"
#include "hcsr_drv.h"

static struct hcsr_dev hcsr04_sensor;           /**< HCSR04 sensor for distance measurement */

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
        int ret;
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

        if (hcsr_lock(&hcsr04_sensor))
                return -EBUSY;

        // [ToDo] Check Max7219 is busy or not. (Lock in the work function).

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

        // Remove isr and irq_no
        hcsr_isr_exit(&hcsr04_sensor);

        // Assign to the device.
        hcsr04_sensor.settings.pins.trigger_pin = -1;
        hcsr04_sensor.settings.pins.echo_pin = -1;
        
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

        // Assign to the device.
        hcsr04_sensor.settings.pins.trigger_pin = pins.trigger;
        hcsr04_sensor.settings.pins.echo_pin = pins.echo;

        // Setup isr and irq_no
        ret = hcsr_isr_init(&hcsr04_sensor);
        if (ret < 0) {
                hcsr04_sensor.irq_no = 0;
                quark_gpio_fini_pin(hcsr04_sensor.settings.pins.echo_pin);
                hcsr04_sensor.settings.pins.trigger_pin = -1;
                hcsr04_sensor.settings.pins.echo_pin = -1;
                printk(KERN_ALERT "irq failed. %d\n", ret);
        }
        hcsr_unlock(&hcsr04_sensor);

        max7219_device_config(s_pins.chip_select);
        //[ToDo] unlock Max7219
        return ret;
}

static int genl_hb_rx_measure_msg(struct sk_buff* skb, struct genl_info* info) {
        
        if (!info->attrs[GENL_HB_ATTR_MSG]) {
                printk(KERN_ERR "empty message from %d!!\n", info->snd_portid);
                printk(KERN_ERR "%p\n", info->attrs[GENL_HB_ATTR_MSG]);
                return -EINVAL;
        }

        printk(KERN_INFO "length:%d\n", nla_len(info->attrs[GENL_HB_ATTR_MSG]));

        printk(KERN_NOTICE "Going to do the measurement.\n");

        // Save context before starting a measurement.
        hcsr04_sensor.on_complete.context = info->snd_portid;
        hcsr04_sensor.on_complete.notify = genl_hb_tx_distance_msg;

        // Invoke HCSR module to measure the distance.
        // lock the device
        if (hcsr_lock(&hcsr04_sensor))
                return -EBUSY;

        // start a new sampling by starting a new thread
        hcsr_new_task(&hcsr04_sensor);
        
        return 0;
}

static int genl_hb_rx_display_msg(struct sk_buff* skb, struct genl_info* info) {
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
        max7219_send_msg(pattern.led, sizeof(genl_hb_pattern_t), s_pins.chip_select);

        return 0;
}

static void genl_hb_tx_distance_msg(unsigned long data) {   
        void *hdr;
        u32 portid = hcsr04_sensor.on_complete.context;
        int res, flags = GFP_ATOMIC;
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

        res = nla_put_u64(skb, GENL_HB_ATTR_DIS, data);
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

        // Initialize MAX7219 LED Driver.
        ret = max7219_device_init();
        if (ret)
                return -EINVAL;

        // Initialize HCSR-04 Ultrasonic Driver.
        ret = hcsr_init_one(&hcsr04_sensor);
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

        // Remove HCSR04-04 Ultrasonic Driver.
        hcsr_fini_one(&hcsr04_sensor);

        if (s_pins.chip_select != -1) {
                quark_gpio_fini_pin(s_pins.chip_select);
                s_pins.chip_select = -1;
        }
}

module_init(heartbeat_init);
module_exit(heartbeat_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xiangyu Guo");
MODULE_DESCRIPTION("Assignment 3");