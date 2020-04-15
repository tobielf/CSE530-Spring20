/**
 * @file hcsr_drv.h
 * @brief Low level driver of hcsr04 device.
 * 
 * @author Xiangyu Guo
 */
#ifndef __HCSR_DRV_H__
#define __HCSR_DRV_H__

#include "defs.h"

/**
 * @brief lock the device.
 * @param devp, a valid device pointer.
 * @return 0, successfully get the lock, otherwise -EBUSY indicates the device
 *         is busy.
 */
int hcsr_lock(hcsr_dev_t *);

/**
 * @brief unlock the device.
 * @param devp, a valid device pointer.
 */
void hcsr_unlock(hcsr_dev_t *);

/**
 * @brief create a new sampling task.
 * @param devp, a valid device pointer.
 * @return 0, on success; otherwise failure.
 */
int hcsr_new_task(hcsr_dev_t *);

/**
 * @brief reallocate the sampling buffer
 * @param devp, a valid pointer to device object.
 * @param m, new buffer size.
 * @return 0 on success, otherwise failed.
 */
int hcsr_reallocate(hcsr_dev_t *, int);

/**
 * @brief initialize the interrupt service routine.
 * @param devp, a valid pointer to device object.
 */
int hcsr_isr_init(hcsr_dev_t *);

/**
 * @brief unregister the interrupt service routine.
 * @param devp, a valid pointer to device object.
 */
int hcsr_isr_exit(hcsr_dev_t *);

int hcsr_init_one(struct hcsr_dev *devp);

void hcsr_fini_one(struct hcsr_dev *devp);

#endif