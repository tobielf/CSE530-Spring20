/**
 * @file common.h
 * @brief shared structure definition and macro between kernel and user space
 * @author Xiangyu Guo
 */
#ifndef __HEARTBEAT_COMMON_H__
#define __HEARTBEAT_COMMON_H__

#include <linux/netlink.h>

#ifndef __KERNEL__
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <stdint.h>
#endif

#define GENL_HB_FAMILY_NAME       "genl_hb"

#define GENL_HB_ATTR_MSG_MAX      256

/** Message types */
enum {
    GENL_HB_C_UNSPEC,           /**< Must NOT use element 0 */
    GENL_HB_CONFIG_MSG,         /**< Pin configuration message */
    GENL_HB_MEASURE_MSG,        /**< Distance measurement message */
    GENL_HB_DISPLAY_MSG,        /**< Pattern display message */
};

/** Generic netlink attributes */
enum genl_hb_attrs {
    GENL_HB_ATTR_UNSPEC,        /**< Must NOT use element 0 */
    GENL_HB_ATTR_MSG,           /**< Only one attribute, message */
    __GENL_HB_ATTR__MAX,
};
#define GENL_HB_ATTR_MAX (__GENL_HB_ATTR__MAX - 1)

typedef struct genl_hb_pins {
    int chip_select;
    int trigger;
    int echo;
} genl_hb_pins_t;

typedef struct genl_hb_pattern {
    uint8_t led[8];
} genl_hb_pattern_t;

static struct nla_policy genl_hb_policy[GENL_HB_ATTR_MAX+1] = {
        [GENL_HB_ATTR_MSG] = {
                .type = NLA_STRING,
#ifdef __KERNEL__
                .len = GENL_HB_ATTR_MSG_MAX
#else
                .maxlen = GENL_HB_ATTR_MSG_MAX
#endif
    },
};

#endif