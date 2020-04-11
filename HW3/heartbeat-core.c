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

#include <net/genetlink.h>

#include "common.h"

#define GENL_HB_HELLO_INTERVAL        5000          /**< Timeout to send reply */

static struct timer_list timer;                     /**< Timer for the delay */

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
                printk(KERN_ERR "empty message from %d!!\n",
                info->snd_portid);
                printk(KERN_ERR "%p\n", info->attrs[GENL_HB_ATTR_MSG]);
                return -EINVAL;
        }

        printk(KERN_INFO "length:%d\n", nla_len(info->attrs[GENL_HB_ATTR_MSG]));

        memcpy(&pins, nla_data(info->attrs[GENL_HB_ATTR_MSG]), 8);
        printk(KERN_NOTICE "%u says trigger:%d echo:%d\n", info->snd_portid,
                                pins.trigger, pins.echo);

        // [ToDo] Invoke HCSR module to configure pins.
        return 0;
}

static int genl_hb_rx_measure_msg(struct sk_buff* skb, struct genl_info* info) {
        printk(KERN_INFO "length:%d\n", nla_len(info->attrs[GENL_HB_ATTR_MSG]));

        printk(KERN_NOTICE "Going to do the measurement.\n");

        timer.data = (unsigned long)info->snd_portid;
        timer.function = genl_hb_tx_distance_msg;
        timer.expires = jiffies + msecs_to_jiffies(GENL_HB_HELLO_INTERVAL);
        add_timer(&timer);

        // [ToDo] Invoke HCSR module to measure the distance.

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

        // Register Netlink family, so the driver is ready to work.
        ret = genl_register_family(&genl_hb_family);
        if (ret)
                return -EINVAL;

        return 0;
}

static void heartbeat_exit(void) {
        del_timer(&timer);
        // Unregister Netlink family.
        genl_unregister_family(&genl_hb_family);
}

module_init(heartbeat_init);
module_exit(heartbeat_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xiangyu Guo");
MODULE_DESCRIPTION("Assignment 3");