/**
 * @file hcsr.h
 * @brief High level driver of hcsr04 definitions.
 * 
 * @author Xiangyu Guo
 */
#ifndef __HCSR_H__
#define __HCSR_H__

#include "hcsr04.h"
#include "ring_buff.h"

#include "common.h"

#define BUFF_SIZE       (16)                    /**< Name buffer size */
#define NUM_OF_OUTLIER  (2)                     /**< Outliers we are going to remove */
#define MIN_INTERVAL    (60)                    /**< Minimal sampling interval in ms */


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