/**
 * @file common.h
 * @brief shared structure definition and macro between kernel and user space
 * @author Xiangyu Guo
 */
#ifndef __COMMON_H__
#define __COMMON_H__
#include <linux/ioctl.h>

typedef struct pins_setting {
        int trigger_pin;                /**< GPIO# of trigger pin */
        int echo_pin;                   /**< GPIO# of echo pin */
} pins_setting_t;

typedef struct parameters_setting {
        unsigned int m;                 /**< Number of samples */
        unsigned int delta;             /**< Interval of sampling */
} parameters_setting_t;

#define CONFIG_PINS _IOWR('d', 1, pins_setting_t *)
#define SET_PARAMETERS _IOW('d', 2, parameters_setting_t *)

typedef struct result_info {
        unsigned long long measurement;         /**< Distance measurement */
        unsigned long long timestamp;           /**< Time stamp (x86 TSC) */
} result_info_t;

#endif