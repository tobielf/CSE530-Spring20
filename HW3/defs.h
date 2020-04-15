/**
 * @file hcsr.h
 * @brief High level driver of hcsr04 definitions.
 * 
 * @author Xiangyu Guo
 */
#ifndef __DEFS_H__
#define __DEFS_H__

#include <linux/miscdevice.h>

#include "ring_buff.h"

#define BUFF_SIZE       (16)                    /**< Name buffer size */
#define NUM_OF_OUTLIER  (2)                     /**< Outliers we are going to remove */
#define MIN_INTERVAL    (60)                    /**< Minimal sampling interval in ms */

typedef struct pins_setting {
        int trigger_pin;                        /**< GPIO# of trigger pin */
        int echo_pin;                           /**< GPIO# of echo pin */
} pins_setting_t;

typedef struct parameters_setting {
        unsigned int m;                         /**< Number of samples */
        unsigned int delta;                     /**< Interval of sampling */
} parameters_setting_t;

typedef struct result_info {
        unsigned long long measurement;         /**< Distance measurement */
        unsigned long long timestamp;           /**< Time stamp (x86 TSC) */
} result_info_t;

typedef struct hcsr_dev hcsr_dev_t;
struct hcsr_dev;

typedef struct sample_data {
        unsigned int count;                     /**< Number of samples */
        unsigned long long *data;               /**< Sampling data storage */
} sample_data_t;

struct hcsr04_sysfs {
        atomic_t most_recent;                   /**< Latest measurement */
        parameters_setting_t params;            /**< Device parameters */
        pins_setting_t pins;                    /**< Device pin settings */
        int endless;                            /**< Nonstop measurement */
};

/** per device structure */
struct hcsr_dev {
        struct miscdevice miscdev;              /**< The miscdevice structure */
        char name[BUFF_SIZE];                   /**< The name of the device */
        struct hcsr04_sysfs settings;           /**< Sysfs settings */
        int irq_no;                             /**< IRQ number for device */
        mp_ring_buff_t *result_queue;           /**< FIFO result_queue queue */
        sample_data_t sample_result;            /**< Storage for sample result */
        atomic_t available;                     /**< Device singlton variable */
        struct task_struct *tsk;                /**< Sampling thread */
        int job_done;                           /**< Thread job flag */
};

#endif