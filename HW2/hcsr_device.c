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

#define NAME_LEN (8)                            /**< The length of device name */
#define NUM_OF_DEVICES (10)                     /**< Number of devices supported by this driver*/

static const char *platform_name = "HCSR";      /**< Constant platform name */

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

        if (n > NUM_OF_DEVICES) {
                printk(KERN_WARNING "Only supported %d devices.\n", NUM_OF_DEVICES);
                n = NUM_OF_DEVICES;
        }

        hcsr_devices = kzalloc(n * sizeof(hcsr_device_t), GFP_KERNEL);
        if (hcsr_devices == NULL) {
                return -ENOMEM;
        }

        // Register the device
        for (i = 0; i < n; i++) {
                char *dev_name = kmalloc(NAME_LEN, GFP_KERNEL);
                snprintf(dev_name, NAME_LEN, "%s_%d", platform_name, i);
                hcsr_devices[i].plf_dev.driver_override = DRIVER_NAME;
                hcsr_devices[i].plf_dev.name          = dev_name;
                hcsr_devices[i].plf_dev.id            = PLATFORM_DEVID_NONE;
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
                platform_device_unregister(&hcsr_devices[i].plf_dev);
                kfree(hcsr_devices[i].plf_dev.name);
        }
        kfree(hcsr_devices);

        printk(KERN_ALERT "Goodbye, unregister the devices.\n");
}

module_init(hcsr_device_init);
module_exit(hcsr_device_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xiangyu Guo");
MODULE_DESCRIPTION("Assignment 2-Part2");