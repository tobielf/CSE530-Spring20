/**
 * @file hcsr_device.c
 * @brief hcsr device driver, working with the platform driver.
 * @author Xiangyu Guo
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>

#include <linux/fs.h>

#include <linux/platform_device.h>

#include "hcsr_device.h"

#define CHAR_BUF_LEN (8)                        /**< Length of name buffer */
#define NUM_OF_DEVICES (10)                     /**< Number of devices supported by this driver*/
#define DEVICE_NAME "HCSR"                      /**< Sysfs class name */

static const char *platform_name = "HCSR04";    /**< Constant platform name */

static dev_t dev_num = 0;                       /**< Driver Major Number */
static struct class *s_dev_class = NULL;        /**< Driver Class */

/**
 * @brief release device structure
 * @note dummy implementation
 */
static void hcsr_device_release(struct device *dev) {
        // do nothing
        //http://lists.infradead.org/pipermail/linux-pcmcia/2004-March/000598.html
};

/**< hcsr devices description */
static hcsr_device_t *hcsr_devices = NULL;

/** Device parameter n */
static int n = 1;
module_param(n, int, S_IRUGO);

/**
 * @brief register the device when module is initiated
 * @return 0 on success, otherwise failed.
 */
static int hcsr_device_init(void)
{
        int i;
        int ret;
        // Get a device number for the driver
        ret = alloc_chrdev_region(&dev_num, 0, n, DEVICE_NAME);

        if (n > NUM_OF_DEVICES) {
                printk(KERN_WARNING "Only supported %d devices.\n", NUM_OF_DEVICES);
                n = NUM_OF_DEVICES;
        }

        hcsr_devices = kmalloc(n * sizeof(hcsr_device_t), GFP_KERNEL);
        if (hcsr_devices == NULL) {
                return -ENOMEM;
        }

        s_dev_class = class_create(THIS_MODULE, DEVICE_NAME);

        // Register the device
        for (i = 0; i < n; i++) {
                char *dev_name = kmalloc(CHAR_BUF_LEN, GFP_KERNEL);
                snprintf(dev_name, CHAR_BUF_LEN, "%s_%d", DEVICE_NAME, i);
                hcsr_devices[i].name                  = dev_name;
                hcsr_devices[i].dev_class             = s_dev_class;
                hcsr_devices[i].dev_no                = MKDEV(MAJOR(dev_num), i);
                hcsr_devices[i].plf_dev.name          = platform_name;
                hcsr_devices[i].plf_dev.id            = 0;
                hcsr_devices[i].plf_dev.dev.release   = hcsr_device_release;
                platform_device_register(&hcsr_devices[i].plf_dev);
        }

        return 0;
}

/**
 * @brief remove the device when module is removed
 */
static void hcsr_device_exit(void)
{
        int i;

        for (i = 0; i < n; i++) {
                kfree(hcsr_devices[i].name);
                platform_device_unregister(&hcsr_devices[i].plf_dev);
        }
        kfree(hcsr_devices);

        // Destroy driver_class
        class_destroy(s_dev_class);

        // Release the major number
        unregister_chrdev_region(dev_num, n);

        printk(KERN_ALERT "Goodbye, unregister the device\n");
}

module_init(hcsr_device_init);
module_exit(hcsr_device_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xiangyu Guo");
MODULE_DESCRIPTION("Assignment 2-Part2");