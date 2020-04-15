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

#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/kthread.h>

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/fs.h>

#include <asm/div64.h>

#include <linux/platform_device.h>

#include "defs.h"
#include "hcsr_device.h"
#include "hcsr_drv.h"
#include "hcsr_config.h"
#include "hcsr_sysfs.h"
#include "ring_buff.h"
#include "common.h"
#include "utils.h"

#define DEVICE_NAME_PREFIX      "HCSR_"         /**< Device prefix */
#define CLASS_NAME              "HCSR"          /**< Class name in sysfs */

static struct class_compat *s_dev_class = NULL; /**< Driver Compatible Class */

#ifdef NORMAL_MODULE
static struct hcsr_dev *dev;                    /**< Per device objects */

/** Device parameter n */
static int n = 1;
module_param(n, int, S_IRUGO);
#endif //NORMAL_MODULE

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

/** File operations supported by this driver */
struct file_operations fops = {
        .owner = THIS_MODULE,
        .open = hcsr_open,
        .release = hcsr_release,
        .read = hcsr_read,
        .write = hcsr_write,
        .unlocked_ioctl = hcsr_ioctl
};

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
        if (devp->settings.pins.trigger_pin == -1 || devp->settings.pins.echo_pin == -1)
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
                        msleep(devp->settings.params.m * (devp->settings.params.delta));
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
        if (devp->settings.pins.trigger_pin == -1 || devp->settings.pins.echo_pin == -1)
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
                        if (hcsr04_config_fini(&devp->settings.pins))
                                goto failed;

                        // Assign to the device.
                        devp->settings.pins.trigger_pin = -1;
                        devp->settings.pins.echo_pin = -1;
                        
                        // Configure pins.
                        if (hcsr04_config_init(&pins))
                                goto failed;

                        // Assign to the device.
                        devp->settings.pins.trigger_pin = pins.trigger_pin;
                        devp->settings.pins.echo_pin = pins.echo_pin;

                        // Setup isr and irq_no
                        ret = hcsr_isr_init(devp);
                        if (ret < 0) {
                                devp->irq_no = 0;
                                hcsr04_config_fini(&devp->settings.pins);
                                devp->settings.pins.trigger_pin = -1;
                                devp->settings.pins.echo_pin = -1;
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
                        devp->settings.params.delta = params.delta;

                        // Reallocate the sampling buffer.
                        if (hcsr_reallocate(devp, params.m + NUM_OF_OUTLIER))
                                goto failed;
                        // Assign to the device.
                        devp->settings.params.m = params.m + NUM_OF_OUTLIER;
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
                
                // Create miscdev
                dev[i].miscdev.minor = MISC_DYNAMIC_MINOR;
                dev[i].miscdev.name = dev[i].name;
                dev[i].miscdev.fops = &fops;
                ret = misc_register(&dev[i].miscdev);
                if (ret) {
                        printk(KERN_ALERT "misc_register failed\n");
                        // ToDo: clear up before return an error.
                        return ret;
                }

                // Register the device driver to the file system.
                class_compat_create_link(s_dev_class, dev[i].miscdev.this_device, NULL);

                sysfs_create_groups(&(dev[i].miscdev.this_device->kobj), hcsr_groups);

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
                // Unregister the device driver from the file system.
                sysfs_remove_groups(&(dev[i].miscdev.this_device->kobj), hcsr_groups);

                class_compat_remove_link(s_dev_class, dev[i].miscdev.this_device, NULL);

                // Remove from miscdev chain
                misc_deregister(&dev[i].miscdev);

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