/**
 * @file hcsr_device.h
 * @brief device driver definition.
 * @author Xiangyu Guo
 */
#include <linux/platform_device.h>

#include "hcsr04.h"

#ifndef __HCSR04_DEVICE_H__
#define __HCSR04_DEVICE_H__

typedef struct hcsr_device {
        hcsr_dev_t *dev;                    /**< Device structure */
        struct class_compat *dev_class;     /**< Sysfs class */
        struct platform_device plf_dev;     /**< Platform device */
} hcsr_device_t;

#endif