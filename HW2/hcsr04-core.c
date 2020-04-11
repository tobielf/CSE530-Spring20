/**
 * @file hcsr04-core.c
 * @brief High level driver of hcsr04 device
 * 
 * @author Xiangyu Guo
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/major.h>

#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/kthread.h>

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/fs.h>

#include <linux/uaccess.h>
#include <asm/uaccess.h>
#include <asm/div64.h>

#include <linux/platform_device.h>

#include "hcsr04.h"
#include "hcsr_device.h"
#include "utils.h"
#include "ring_buff.h"
#include "hcsr_config.h"
#include "common.h"

#define DEVICE_NAME_PREFIX      "HCSR_"         /**< Device prefix */
#define CLASS_NAME              "HCSR"          /**< Class name in sysfs */
#define BUFF_SIZE       (16)                    /**< Name buffer size */
#define HISTORY_SIZE    (5)                     /**< Sampling history size */
#define NUM_OF_OUTLIER  (2)                     /**< Outliers we are going to remove */
#define MIN_INTERVAL    (60)                    /**< Minimal sampling interval in ms */

typedef struct sample_data {
        unsigned int count;                     /**< Number of samples */
        unsigned long long *data;               /**< Sampling data storage */
} sample_data_t;

/** per device structure */
struct hcsr_dev {
        struct miscdevice miscdev;              /**< The miscdevice structure */
        char name[BUFF_SIZE];                   /**< The name of the device */
        parameters_setting_t params;            /**< Device parameters */
        pins_setting_t pins;                    /**< Device pin settings */
        int irq_no;                             /**< IRQ number for device */
        mp_ring_buff_t *result_queue;           /**< FIFO result_queue queue */
        sample_data_t sample_result;            /**< Storage for sample result */
        atomic_t available;                     /**< Device singlton variable */
        struct device *s_dev;                   /**< Sysfs device structure */
        struct task_struct *tsk;                /**< Sampling thread */
        int job_done;                           /**< Thread job flag */
        int endless;                            /**< Nonstop measurement */
        atomic_t most_recent;                   /**< Latest measurement */
};

static struct class_compat *s_dev_class = NULL; /**< Driver Compatible Class */

#ifdef NORMAL_MODULE
static struct hcsr_dev *dev;                    /**< Per device objects */

/** Device parameter n */
static int n = 1;
module_param(n, int, S_IRUGO);
#endif //NORMAL_MODULE

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
 * @brief open operation of the device
 * @param i, the inode representation on the file system
 * @param filp, file pointer to the file.
 * @return 0, on success, otherwise failure.
 */
static int hcsr_open(struct inode *, struct file *);

/**
 * @brief close operation of the device
 * @param i, the inode representation on the file system
 * @param filp, file pointer to the file.
 * @return 0, on success, otherwise failure.
 */
static int hcsr_release(struct inode *, struct file *);

/**
 * @brief read operation of the device
 * @param filp, file pointer to the file.
 * @param buf, the buffer passing from user space.
 * @param count, the buffer size.
 * @param pos, the offset of the file.
 * @return positive, the bytes successfully read.
 *         negative, the errno.
 */
static ssize_t hcsr_read(struct file *, char *, size_t, loff_t *);

/**
 * @brief write operation of the device
 * @param filp, file pointer to the file.
 * @param buf, the buffer passing from user space.
 * @param count, the buffer size.
 * @param pos, the offset of the file.
 * @return positive, the bytes successfully write.
 *         negative, the errno.
 */
static ssize_t hcsr_write(struct file *, const char *, size_t, loff_t *);

/**
 * @brief ioctl operation of the device
 * @param filp, file pointer to the file.
 * @param cmd, the ioctl command.
 * @param arg, the ioctl argument.
 */
static long hcsr_ioctl(struct file *, unsigned int, unsigned long);

/**
 * @brief lock the device.
 * @param devp, a valid device pointer.
 * @return 0, successfully get the lock, otherwise -EBUSY indicates the device
 *         is busy.
 */
static int hcsr_lock(hcsr_dev_t *);

/**
 * @brief unlock the device.
 * @param devp, a valid device pointer.
 */
static void hcsr_unlock(hcsr_dev_t *);

/**
 * @brief create a new sampling task.
 * @param devp, a valid device pointer.
 * @return 0, on success; otherwise failure.
 */
static int hcsr_new_task(hcsr_dev_t *);

/**
 * @brief compute the distance.
 * @param devp, a valid pointer to device object.
 * @return averaged result in centimeter without outlier.
 */
static unsigned long long hcsr_get_pulse_width(hcsr_dev_t *);

/**
 * @brief reallocate the sampling buffer
 * @param devp, a valid pointer to device object.
 * @param m, new buffer size.
 * @return 0 on success, otherwise failed.
 */
static int hcsr_reallocate(hcsr_dev_t *, int);

/**
 * @brief initialize the interrupt service routine.
 * @param devp, a valid pointer to device object.
 */
static int hcsr_isr_init(hcsr_dev_t *);

/**
 * @brief unregister the interrupt service routine.
 * @param devp, a valid pointer to device object.
 */
static int hcsr_isr_exit(hcsr_dev_t *);

/** File operations supported by this driver */
struct file_operations fops = {
        .owner = THIS_MODULE,
        .open = hcsr_open,
        .release = hcsr_release,
        .read = hcsr_read,
        .write = hcsr_write,
        .unlocked_ioctl = hcsr_ioctl
};

/** ==========================================================================
 *                      Distance sysfs attribute
 *============================================================================*/
static ssize_t hcsr_distance_show(struct device *dev,
                                  struct device_attribute *attr,
                                  char *buf) {
        hcsr_dev_t *devp = dev_get_drvdata(dev);
        ssize_t status;

        // get most recently measurement.
        status = sprintf(buf, "%d\n", atomic_read(&devp->most_recent));

        return status;
}

static DEVICE_ATTR(distance, S_IRUSR, hcsr_distance_show, NULL);

/** ==========================================================================
 *                      Trigger sysfs attribute
 *============================================================================*/
static ssize_t hcsr_trigger_show(struct device *dev,
                                 struct device_attribute *attr,
                                 char *buf) {
        hcsr_dev_t *devp = dev_get_drvdata(dev);

        return sprintf(buf, "%d\n", devp->pins.trigger_pin);
}

static ssize_t hcsr_trigger_store(struct device *dev,
                                  struct device_attribute *attr,
                                  const char *buf,
                                  size_t count) {
        int pin;
        int status;
        hcsr_dev_t *devp = dev_get_drvdata(dev);
        status = sscanf(buf, "%d", &pin);
        // lock the device, make sure no measurement using the pin.
        if (hcsr_lock(devp))
                return -EBUSY;
        if (hcsr04_config_validate_pin(pin)) {
                hcsr_unlock(devp);
                return -EINVAL;
        }
        if (devp->pins.trigger_pin != -1)
                hcsr04_fini_pins(devp->pins.trigger_pin);

        devp->pins.trigger_pin = -1;

        if (hcsr04_init_pins(pin, OUTPUT)) {
                hcsr_unlock(devp);
                return -EBUSY;
        }

        devp->pins.trigger_pin = pin;
        // unlock the device, finish pin setting.
        hcsr_unlock(devp);
        return count;
}

static DEVICE_ATTR(trigger, S_IRUSR | S_IWUSR, hcsr_trigger_show, hcsr_trigger_store);

/** ==========================================================================
 *                      Echo sysfs attribute
 *============================================================================*/
static ssize_t hcsr_echo_show(struct device *dev,
                                 struct device_attribute *attr,
                                 char *buf) {
        hcsr_dev_t *devp = dev_get_drvdata(dev);

        return sprintf(buf, "%d\n", devp->pins.echo_pin);
}

static ssize_t hcsr_echo_store(struct device *dev,
                                  struct device_attribute *attr,
                                  const char *buf,
                                  size_t count) {
        int pin;
        int status;
        hcsr_dev_t *devp = dev_get_drvdata(dev);
        status = sscanf(buf, "%d", &pin);
        // lock the device, make sure no measurement using the pin.
        if (hcsr_lock(devp))
                return -EBUSY;
        if (hcsr04_config_validate_pin(pin) ||
            hcsr04_config_validate_echo(pin)) {
                hcsr_unlock(devp);
                return -EINVAL;
        }
        if (devp->pins.echo_pin != -1)
                hcsr04_fini_pins(devp->pins.echo_pin);

        // Remove isr and irq_no
        hcsr_isr_exit(devp);

        devp->pins.echo_pin = -1;

        if (hcsr04_init_pins(pin, INPUT)) {
                hcsr_unlock(devp);
                return -EBUSY;
        }

        devp->pins.echo_pin = pin;

        // Setup isr and irq_no
        status = hcsr_isr_init(devp);
        if (status < 0) {
                devp->irq_no = 0;
                hcsr04_fini_pins(devp->pins.echo_pin);
                devp->pins.echo_pin = -1;
                printk(KERN_ALERT "irq failed %d\n", status);
                hcsr_unlock(devp);
                return status;
        }

        // unlock the device, finish pin setting.
        hcsr_unlock(devp);
        return count;
}

static DEVICE_ATTR(echo, S_IRUSR | S_IWUSR, hcsr_echo_show, hcsr_echo_store);

/** ==========================================================================
 *                      Samples sysfs attribute
 *============================================================================*/
static ssize_t hcsr_samples_show(struct device *dev,
                                 struct device_attribute *attr,
                                 char *buf) {
        hcsr_dev_t *devp = dev_get_drvdata(dev);

        return sprintf(buf, "%d\n", devp->params.m - NUM_OF_OUTLIER);
}

static ssize_t hcsr_samples_store(struct device *dev,
                                  struct device_attribute *attr,
                                  const char *buf,
                                  size_t count) {
        int m;
        int status;
        hcsr_dev_t *devp = dev_get_drvdata(dev);
        status = sscanf(buf, "%d", &m);
        if (m < 1)
                return -EINVAL;
        // lock the device 
        if (hcsr_lock(devp))
                return -EBUSY;
        // update m
        if (hcsr_reallocate(devp, m + NUM_OF_OUTLIER)) {
                hcsr_unlock(devp);
                return -ENOMEM;
        }

        devp->params.m = m + NUM_OF_OUTLIER;

        // unlock the device
        hcsr_unlock(devp);
        return count;
}

static DEVICE_ATTR(number_samples, S_IRUSR | S_IWUSR, hcsr_samples_show, hcsr_samples_store);

/** ==========================================================================
 *                      Interval sysfs attribute
 *============================================================================*/
static ssize_t hcsr_interval_show(struct device *dev,
                                 struct device_attribute *attr,
                                 char *buf) {
        hcsr_dev_t *devp = dev_get_drvdata(dev);

        return sprintf(buf, "%d\n", devp->params.delta);
}

static ssize_t hcsr_interval_store(struct device *dev,
                                  struct device_attribute *attr,
                                  const char *buf,
                                  size_t count) {
        int val;
        hcsr_dev_t *devp = dev_get_drvdata(dev);

        sscanf(buf, "%d", &val);
        if (val < MIN_INTERVAL)
                return -EINVAL;
        // lock the device 
        if (hcsr_lock(devp))
                return -EBUSY;

        devp->params.delta = val;

        // unlock the device
        hcsr_unlock(devp);
        return count;
}

static DEVICE_ATTR(sampling_period, S_IRUSR | S_IWUSR, hcsr_interval_show, hcsr_interval_store);

/** ==========================================================================
 *                      Enable sysfs attribute
 *============================================================================*/
static ssize_t hcsr_enable_show(struct device *dev,
                                 struct device_attribute *attr,
                                 char *buf) {
        hcsr_dev_t *devp = dev_get_drvdata(dev);

        return sprintf(buf, "%d\n", devp->endless);
}

static ssize_t hcsr_enable_store(struct device *dev,
                                  struct device_attribute *attr,
                                  const char *buf,
                                  size_t count) {
        int val;
        hcsr_dev_t *devp = dev_get_drvdata(dev);

        sscanf(buf, "%d", &val);
        if (val) {
                // lock the device 
                if (hcsr_lock(devp))
                        return -EBUSY;
                devp->endless = val;
                hcsr_new_task(devp);
        } else {
                devp->endless = val;
        }
        return count;
}

static DEVICE_ATTR(enable, S_IRUSR | S_IWUSR, hcsr_enable_show, hcsr_enable_store);

static struct attribute *hcsr_attrs[] = {
        &dev_attr_distance.attr,
        &dev_attr_trigger.attr,
        &dev_attr_echo.attr,
        &dev_attr_number_samples.attr,
        &dev_attr_sampling_period.attr,
        &dev_attr_enable.attr,
        NULL,
};

static const struct attribute_group hcsr_group = {
        .attrs = hcsr_attrs,
};

static const struct attribute_group *hcsr_groups[] = {
        &hcsr_group,
        NULL
};

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
                delta = devp->params.delta;
                trigger_pin = hcsr04_shield_to_gpio(devp->pins.trigger_pin);

                do {
                        m = devp->params.m;
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
                        atomic_set(&devp->most_recent, res->measurement);
                } while (devp->endless);

                devp->job_done = 1;

                // unlock device
                hcsr_unlock(devp);
        }
        do_exit(0);
        return 0;
}

static int hcsr_lock(hcsr_dev_t *devp) {
        if (!atomic_dec_and_test(&devp->available)) {
                atomic_inc(&devp->available);
                return -EBUSY;
        }
        return 0;
}

static void hcsr_unlock(hcsr_dev_t *devp) {
        atomic_inc(&devp->available);
}

static int hcsr_new_task(hcsr_dev_t *devp) {
        // start a new sampling by starting a new thread
        devp->job_done = 0;
        return 0;
}

static unsigned long long hcsr_get_pulse_width(hcsr_dev_t *devp) {
        int i;
        unsigned long long sum = 0;
        unsigned long long large = 0;
        unsigned long long small = UINT_MAX;

        // Go through the sampling result buffer.
        for (i = 0; i < devp->params.m * 2; i+=2) {
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

        do_div(sum, (devp->params.m - NUM_OF_OUTLIER));
        do_div(sum, 400);
        do_div(sum, 58);

        return sum;
}

static int hcsr_reallocate(hcsr_dev_t *devp, int m) {
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

static int hcsr_isr_init(hcsr_dev_t *devp) {
        // Trigger and waiting for the response.
        devp->irq_no = gpio_to_irq(hcsr04_shield_to_gpio(devp->pins.echo_pin));
        printk(KERN_INFO "irq no is: %d\n", devp->irq_no);

        // Check return value.
        return request_any_context_irq(devp->irq_no, isr_handler, 
                                IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
                                "hcsr04", (void *)&devp->sample_result);
}

static int hcsr_isr_exit(hcsr_dev_t *devp) {
        if (devp->irq_no)
                free_irq(devp->irq_no, (void *)&devp->sample_result);
        return 0;
}

static int hcsr_open(struct inode *i, struct file *filp) {
        struct hcsr_dev *devp;

        devp = container_of(filp->private_data, struct hcsr_dev, miscdev);

        filp->private_data = devp;
        return 0;
}

static int hcsr_release(struct inode *i, struct file *filp) {
        return 0;
}

static ssize_t hcsr_read(struct file *filp, char *buf,
                 size_t count, loff_t *ppos) {
        hcsr_dev_t *devp = (hcsr_dev_t *)filp->private_data;
        result_info_t *result;

        // Invoke ioctl to setup the pins first.
        if (devp->pins.trigger_pin == -1 || devp->pins.echo_pin == -1)
                return -EINVAL;

        // Security: comparing the count with sizeof(result), take the min one.
        count = min(count, sizeof(result_info_t));

        // Ring buffer is empty? (Block I/O)
        if (ring_buff_get(devp->result_queue, (void **)&result)) {
                printk(KERN_INFO "No result, going to request one\n");
                // Someone working on it?
                if (!hcsr_lock(devp)) {
                        // None, start a new task.
                        hcsr_new_task(devp);
                }
                // Waiting for the sampling complete and read again.
                while (ring_buff_get(devp->result_queue, (void **)&result)) {
                        msleep(devp->params.m * (devp->params.delta));
                }
        }

        // Copy the result to user.
        if (copy_to_user(buf, result, count))
                return -EFAULT;

        kfree(result);

        return 0;
}

static ssize_t hcsr_write(struct file *filp, const char *buf,
                          size_t count, loff_t *ppos) {
        int value;

        hcsr_dev_t *devp = (hcsr_dev_t *)filp->private_data;

        printk(KERN_INFO "count: %d sizeof: %d\n", count, sizeof(int));

        // Invoke ioctl to setup the pins first.
        if (devp->pins.trigger_pin == -1 || devp->pins.echo_pin == -1)
                return -EINVAL;

        // Security: comparing the count with sizeof(int)
        count = min(count, sizeof(int));

        // get data from user
        if (copy_from_user(&value, buf, count))
                return -EFAULT;

        // lock the device
        if (hcsr_lock(devp))
                return -EBUSY;

        printk(KERN_INFO "The value is %d\n", value);
        // clear all result_queue if the value is non-0.
        if (value) {
                printk(KERN_ALERT "Going to clean up the ring buff\n");
                ring_buff_removeall(devp->result_queue);
        }

        // start a new sampling by starting a new thread
        hcsr_new_task(devp);

        return 0;
}

static long hcsr_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
        hcsr_dev_t *devp = filp->private_data;
        pins_setting_t pins;
        parameters_setting_t params;
        int ret;

        // Not allow ioctl configuration during the sampling job.
        if (hcsr_lock(devp))
                return -EBUSY;

        switch (cmd) {
                case CONFIG_PINS:
                        if (copy_from_user(&pins, (pins_setting_t *)arg, 
                                           sizeof(pins_setting_t)))
                                goto failed;
                        // Validate the pins.
                        if (hcsr04_config_validate(&pins))
                                goto failed;

                        // Remove isr and irq_no
                        hcsr_isr_exit(devp);
                        
                        // Remove previous GPIO setup
                        if (hcsr04_config_fini(&devp->pins))
                                goto failed;

                        // Assign to the device.
                        devp->pins.trigger_pin = -1;
                        devp->pins.echo_pin = -1;
                        
                        // Configure pins.
                        if (hcsr04_config_init(&pins))
                                goto failed;

                        // Assign to the device.
                        devp->pins.trigger_pin = pins.trigger_pin;
                        devp->pins.echo_pin = pins.echo_pin;

                        // Setup isr and irq_no
                        ret = hcsr_isr_init(devp);
                        if (ret < 0) {
                                devp->irq_no = 0;
                                hcsr04_config_fini(&devp->pins);
                                devp->pins.trigger_pin = -1;
                                devp->pins.echo_pin = -1;
                                printk(KERN_ALERT "irq failed. %d\n", ret);
                                goto failed;
                        }

                        break;
                case SET_PARAMETERS:
                        if (copy_from_user(&params, (parameters_setting_t *)arg, 
                                           sizeof(parameters_setting_t)))
                                goto failed;
                        // According to the datasheet, the period should be 60ms.
                        if (params.delta < MIN_INTERVAL) {
                                printk(KERN_ALERT "Should have at least 60ms\n");
                                goto failed;
                        }
                        devp->params.delta = params.delta;

                        // Reallocate the sampling buffer.
                        if (hcsr_reallocate(devp, params.m + NUM_OF_OUTLIER))
                                goto failed;
                        // Assign to the device.
                        devp->params.m = params.m + NUM_OF_OUTLIER;
                        break;
                default:
                        hcsr_unlock(devp);
                        return -EINVAL;
        }

        // Unlock the device
        hcsr_unlock(devp);
        return 0;
failed:
        hcsr_unlock(devp);
        return -EFAULT;
}

static int hcsr_init_one(struct hcsr_dev *devp) {
        int ret;

        printk(KERN_ALERT "Found the device -- %s\n", devp->name);
        printk(KERN_INFO "Creating %s\n", devp->name);

        // Initialized device lock.
        atomic_set(&devp->available, 1);
        atomic_set(&devp->most_recent, 0);

        // Initialized default m and delta.
        devp->params.m = 4;
        devp->params.delta = 200;

        // Initialized default pins.
        devp->pins.trigger_pin = -1;
        devp->pins.echo_pin = -1;

        devp->irq_no = 0;
        devp->job_done = 1;
        devp->endless = 0;

        // Initialized the result_queue buff.
        devp->result_queue = ring_buff_init(HISTORY_SIZE, kfree); 
        if (devp->result_queue == NULL)
                return -ENOMEM;

        // Initialized the sample_result buff.
        devp->sample_result.data = kmalloc(sizeof(unsigned long long) * 
                                        devp->params.m * 2, GFP_KERNEL);
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

        // Create miscdev
        devp->miscdev.minor = MISC_DYNAMIC_MINOR;
        devp->miscdev.name = devp->name;
        devp->miscdev.fops = &fops;
        ret = misc_register(&devp->miscdev);
        if (ret) {
                printk(KERN_ALERT "misc_register failed\n");
                // ToDo: clear up before return an error.
                return ret;
        }

        printk(KERN_INFO "Adding %s\n", devp->name);
        // Register the device driver to the file system.
#ifdef NORMAL_MODULE
        class_compat_create_link(s_dev_class, devp->miscdev.this_device, NULL);
#endif
        sysfs_create_groups(&(devp->miscdev.this_device->kobj), hcsr_groups);

        return 0;
}

static void hcsr_fini_one(struct hcsr_dev *devp) {
        // Unregister the device driver from the file system.
        sysfs_remove_groups(&(devp->miscdev.this_device->kobj), hcsr_groups);

#ifdef NORMAL_MODULE
        class_compat_remove_link(s_dev_class, devp->miscdev.this_device, NULL);
#endif
        // Remove isr and irq_no
        hcsr_isr_exit(devp);

        // Release the gpio setting.
        hcsr04_config_fini(&devp->pins);

        devp->endless = 0;

        // Exit the thread.
        kthread_stop(devp->tsk);

        // Release the sample_result buff.
        kfree(devp->sample_result.data);

        // Release the result_queue buff.
        ring_buff_fini(devp->result_queue);

        // Remove from miscdev chain
        misc_deregister(&devp->miscdev);
}

#ifdef NORMAL_MODULE
static int hcsr04_init(void) {
        int i;
        int ret;

        printk(KERN_INFO "Registering %d Devices\n", n);

        // Check the param n, can't more than 10, since we only got 20 shield pins.
        if (n > 10)
                return -EINVAL;

        // Allocate space for the structure
        dev = kzalloc(sizeof(struct hcsr_dev) * n, GFP_KERNEL);
        if (dev == NULL) {
                return -ENOMEM;
        }

        // Create a compatible class for device object
        s_dev_class = class_compat_register(CLASS_NAME);

        for (i = 0; i < n; ++i) {
                snprintf(dev[i].name, BUFF_SIZE, "%s%d", DEVICE_NAME_PREFIX, i);
                ret = hcsr_init_one(&dev[i]);
                if (ret) {
                        return ret;
                }
        }

        return 0;
}

static void hcsr04_exit(void) {
        int i;
        printk(KERN_ALERT "Goodbye, world\n");
        for (i = 0; i < n; ++i) {
                hcsr_fini_one(&dev[i]);
        }

        // Destroy driver_class
        class_compat_unregister(s_dev_class);

        kfree(dev);
}
#else
static const struct platform_device_id hcsr_id_table[] = {
        { "HCSR04", 0 },
};

static int hcsr_driver_probe(struct platform_device *pdevp)
{
        hcsr_device_t *hdevp;
        hcsr_dev_t *devp;
        int ret;
        
        hdevp = container_of(pdevp, hcsr_device_t, plf_dev);
        hdevp->dev = kzalloc(sizeof(struct hcsr_dev), GFP_KERNEL);
        devp = hdevp->dev;
        
        snprintf(devp->name, BUFF_SIZE, "%s", pdevp->name);
        ret = hcsr_init_one(devp);

        class_compat_create_link(s_dev_class, &pdevp->dev, NULL);

        sysfs_create_files(&(pdevp->dev.kobj), (const struct attribute **)hcsr_attrs);

        return ret;
};

static int hcsr_driver_remove(struct platform_device *pdevp)
{
        hcsr_device_t *hdevp;
        hcsr_dev_t *devp;
        
        hdevp = container_of(pdevp, hcsr_device_t, plf_dev);
        devp = hdevp->dev;
        
        printk(KERN_ALERT "Removing the device -- %s\n", devp->name);

        sysfs_remove_files(&(pdevp->dev.kobj), (const struct attribute **)hcsr_attrs);

        class_compat_remove_link(s_dev_class, &pdevp->dev, NULL);

        hcsr_fini_one(devp);

        kfree(hdevp->dev);

        return 0;
};

static struct platform_driver hcsr_of_driver = {
        .driver         = {
                .name   = DRIVER_NAME,
                .owner  = THIS_MODULE,
        },
        .probe          = hcsr_driver_probe,
        .remove         = hcsr_driver_remove,
        .id_table       = hcsr_id_table,
};

static int hcsr04_init(void) {
        // Create a compatible class for device object
        s_dev_class = class_compat_register(CLASS_NAME);

        platform_driver_register(&hcsr_of_driver);

        return 0;
}
static void hcsr04_exit(void) {
        platform_driver_unregister(&hcsr_of_driver);

        // Destroy driver_class
        class_compat_unregister(s_dev_class);
}
#endif //NORMAL_MODULE

module_init(hcsr04_init);
module_exit(hcsr04_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xiangyu Guo");
MODULE_DESCRIPTION("Assignment 2");