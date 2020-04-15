/**
 * @file hcsr_drv.c
 * @brief Low level driver of hcsr04 device.
 * 
 * @author Xiangyu Guo
 */
#include <linux/kernel.h>

#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/kthread.h>

#include <linux/slab.h>

#include <linux/uaccess.h>
#include <asm/uaccess.h>

#include "hcsr_drv.h"
#include "hcsr_config.h"

#include "utils.h"

#define HISTORY_SIZE    (5)                     /**< Sampling history size */

/**
 * @brief: handling the echo pin interrupt.
 * @param: irq, the irq number of this isr can handle.
 * @param: dev_id, the data of the isr.
 * @return: IRQ_HANDLED, on successfully handled, otherwise IRQ_IGNORED.
 * @note: keep the isr as short as possible and response faster.
 */
static irqreturn_t isr_handler(int irq, void *dev_id) {
        unsigned long tsc = rdtsc();
        sample_data_t *devp = (sample_data_t *)dev_id;

        // Push the data into the device buffer.
        devp->data[devp->count++] = tsc;

        //wake_up_interruptible
        return IRQ_HANDLED;
}

/**
 * @brief deice working thread on sampling.
 * @param data, running data.
 * @return 0 on success, otherwise failed.
 */
static int hcsr_sampling_thread(void *data);

/**
 * @brief compute the distance.
 * @param devp, a valid pointer to device object.
 * @return averaged result in centimeter without outlier.
 */
static unsigned long long hcsr_get_pulse_width(hcsr_dev_t *);

int hcsr_lock(hcsr_dev_t *devp) {
        if (!atomic_dec_and_test(&devp->available)) {
                atomic_inc(&devp->available);
                return -EBUSY;
        }
        return 0;
}

void hcsr_unlock(hcsr_dev_t *devp) {
        atomic_inc(&devp->available);
}

int hcsr_new_task(hcsr_dev_t *devp) {
        // start a new sampling by starting a new thread
        devp->job_done = 0;
        return 0;
}

int hcsr_reallocate(hcsr_dev_t *devp, int m) {
        unsigned long long *data = kmalloc(sizeof(unsigned long long) * m * 2,
                                           GFP_KERNEL);
        if (data == NULL) {
                printk(KERN_ALERT "Out of memory!\n");
                return -ENOMEM;
        }
        kfree(devp->sample_result.data);
        devp->sample_result.data = data;
        return 0;
}

int hcsr_isr_init(hcsr_dev_t *devp) {
        // Trigger and waiting for the response.
        devp->irq_no = gpio_to_irq(hcsr04_shield_to_gpio(devp->settings.pins.echo_pin));
        printk(KERN_INFO "irq no is: %d\n", devp->irq_no);

        // Check return value.
        return request_any_context_irq(devp->irq_no, isr_handler, 
                                IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
                                "hcsr04", (void *)&devp->sample_result);
}

int hcsr_isr_exit(hcsr_dev_t *devp) {
        if (devp->irq_no)
                free_irq(devp->irq_no, (void *)&devp->sample_result);
        return 0;
}

int hcsr_init_one(struct hcsr_dev *devp) {
        printk(KERN_ALERT "Found the device -- %s\n", devp->name);
        printk(KERN_INFO "Creating %s\n", devp->name);

        // Initialized device lock.
        atomic_set(&devp->available, 1);
        atomic_set(&devp->settings.most_recent, 0);

        // Initialized default m and delta.
        devp->settings.params.m = 4;
        devp->settings.params.delta = 200;

        // Initialized default pins.
        devp->settings.pins.trigger_pin = -1;
        devp->settings.pins.echo_pin = -1;

        devp->irq_no = 0;
        devp->job_done = 1;
        devp->settings.endless = 0;

        // Initialized the result_queue buff.
        devp->result_queue = ring_buff_init(HISTORY_SIZE, kfree); 
        if (devp->result_queue == NULL)
                return -ENOMEM;

        // Initialized the sample_result buff.
        devp->sample_result.data = kmalloc(sizeof(unsigned long long) * 
                                        devp->settings.params.m * 2, GFP_KERNEL);
        if (devp->sample_result.data == NULL)
                return -ENOMEM;

        // Initialized the sampling thread
        printk(KERN_INFO "Going to run the thread\n");
        devp->tsk = kthread_run(hcsr_sampling_thread, devp, "sampling thread");
        if (IS_ERR(devp->tsk)) {
                printk(KERN_INFO "kthread failed\n");
                // Create a child process failed.
                return -ECHILD;
        }

        printk(KERN_INFO "Adding %s\n", devp->name);

        return 0;
}

void hcsr_fini_one(struct hcsr_dev *devp) {
        // Remove isr and irq_no
        hcsr_isr_exit(devp);

        // Release the gpio setting.
        hcsr04_config_fini(&devp->settings.pins);

        devp->settings.endless = 0;

        // Exit the thread.
        kthread_stop(devp->tsk);

        // Release the sample_result buff.
        kfree(devp->sample_result.data);

        // Release the result_queue buff.
        ring_buff_fini(devp->result_queue);
}

static int hcsr_sampling_thread(void *data) {
        int m;
        int delta;
        int trigger_pin;
        hcsr_dev_t *devp = (hcsr_dev_t *)data;
        result_info_t *res = NULL;

        while (!kthread_should_stop()) {
                if (devp->job_done) {
                        msleep(100);
                        continue;
                }
                // save parameters.
                delta = devp->settings.params.delta;
                trigger_pin = hcsr04_shield_to_gpio(devp->settings.pins.trigger_pin);

                do {
                        m = devp->settings.params.m;
                        // clear the sampling buffer before sampling.
                        devp->sample_result.count = 0;
                        // tell the compiler don't optimize the above code using Out of Order Execution.
                        barrier();

                        printk(KERN_INFO "m %d, delta %d\n", m, delta);
                        // Now we can safely start trigger m times.
                        while (m > 0) {
                                gpio_set_value_cansleep(trigger_pin, 1);
                                udelay(20);
                                gpio_set_value_cansleep(trigger_pin, 0);
                                msleep(delta);
                                printk(KERN_INFO "sampling %d\n", m);
                                m--;
                        }

                        printk(KERN_ALERT "Total interrupts %u\n", devp->sample_result.count);
                        // similary, we don't want the below code get executed before the sampling completed.
                        barrier();

                        // Collect the tsc in ISR and compute here.
                        res = kmalloc(sizeof(result_info_t), GFP_KERNEL);
                        if (res == NULL) {
                                break;
                        }

                        res->measurement = hcsr_get_pulse_width(devp);
                        res->timestamp = rdtsc();

                        printk(KERN_INFO "Result: %llu\n", res->measurement);

                        // After sampling, we need to add to the result_queue buff.
                        ring_buff_put(devp->result_queue, res);
                        atomic_set(&devp->settings.most_recent, res->measurement);
                } while (devp->settings.endless);

                devp->job_done = 1;

                // unlock device
                hcsr_unlock(devp);
        }
        do_exit(0);
        return 0;
}

static unsigned long long hcsr_get_pulse_width(hcsr_dev_t *devp) {
        int i;
        unsigned long long sum = 0;
        unsigned long long large = 0;
        unsigned long long small = UINT_MAX;

        // Go through the sampling result buffer.
        for (i = 0; i < devp->settings.params.m * 2; i+=2) {
                unsigned long long diff = devp->sample_result.data[i + 1] - 
                                          devp->sample_result.data[i];
                if (diff < 0) {
                        printk(KERN_INFO "Something went wrong\n");
                        return 0;
                }
                printk(KERN_INFO "diff %llu\n", diff);
                large = max(large, diff);
                small = min(small, diff);
                sum += diff;
        }
        printk(KERN_INFO "large: %llu small: %llu sum %llu\n", large, small, sum);
        // Remove outlier.
        sum -= large;
        sum -= small;

        do_div(sum, (devp->settings.params.m - NUM_OF_OUTLIER));
        do_div(sum, 400);
        do_div(sum, 58);

        return sum;
}
