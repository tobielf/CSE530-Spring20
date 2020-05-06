/**
 * @file ht530_drv.c
 * @brief Hash Table in Kernel Space
 * 
 * @author Xiangyu Guo
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/fs.h>

#include <linux/sched.h>

#include <linux/gpio.h>

#include <linux/uaccess.h>
#include <asm/uaccess.h>

#include "common.h"

#include <linux/hashtable.h>

#define DEVICE_NAME_PREFIX "mt530-"
#define CLASS_NAME "chardrv"
#define DEVICE_NUMBER (2)
#define BUFF_SIZE (16)
#define HASH_TABLE_SIZE_BITS (7)

/** per device structure */
struct ht_dev {
        struct cdev cdev;                       /**< The cdev structure */
        char name[BUFF_SIZE];                   /**< Name of the device */
        DECLARE_HASHTABLE(tbl, HASH_TABLE_SIZE_BITS);   /** Hash table */
};

static dev_t dev_num = 0;                       /**< Driver Major Number */
static struct class *s_dev_class = NULL;        /**< Driver Class */
static struct device *s_dev[DEVICE_NUMBER];     /**< FS device nodes */
static struct ht_dev *dev[DEVICE_NUMBER];       /**< Per device objects */
int test_data_section_symbol;
EXPORT_SYMBOL_GPL(test_data_section_symbol);

static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

struct file_operations fops = {
        .owner = THIS_MODULE,
        .open = dev_open,
        .release = dev_release,
        .read = dev_read,
        .write = dev_write
};

static int dev_open(struct inode *i, struct file *filp) {
        struct ht_dev *devp;

        devp = container_of(i->i_cdev, struct ht_dev, cdev);

        filp->private_data = devp;
        printk(KERN_INFO "######## Start a new test case. ########\n");
        return 0;
}

static int dev_release(struct inode *i, struct file *filp) {
        struct ht_dev *devp = filp->private_data;

        printk(KERN_INFO "\n%s is closing\n", devp->name);
        printk(KERN_INFO "######## Finished a test case. ########\n");
        return 0;
}

// Both the read and write methods return a negative value if an
// error occurs. A return value greater than or equal to 0, instead,
// tells the calling program how many bytes have been successfully transferred.
static ssize_t dev_read(struct file *filp, char *buf,
                        size_t count, loff_t *ppos) {
        gpio_free(40);
        return 0;
}

static ssize_t dev_write(struct file *filp, const char *buf,
                size_t count, loff_t *ppos) {
        gpio_request_one(40, GPIOF_OUT_INIT_LOW, NULL);
        printk(KERN_INFO "mt530 write called by %d\n", current->pid);
        gpio_free(40);
        return 0;
}

static int ht530_init(void) {
        int i;
        int ret;
        // Get a device number for the driver
        printk(KERN_INFO "Register Devices\n");
        ret = alloc_chrdev_region(&dev_num, 0, DEVICE_NUMBER, 
                                        DEVICE_NAME_PREFIX);
        // ToDo: check return value

        // Register the device class
        // ToDo: check return value
        s_dev_class = class_create(THIS_MODULE, CLASS_NAME);

        for (i = 0; i < DEVICE_NUMBER; ++i) {
                dev[i] = kmalloc(sizeof(struct ht_dev), GFP_KERNEL);
                if (!dev[i]) {
                        printk("Bad kmalloc\n");
                        return -ENOMEM;
                }
                snprintf(dev[i]->name, BUFF_SIZE, "%s%d", DEVICE_NAME_PREFIX, i);

                // Create hash table
                hash_init(dev[i]->tbl);
                // Create cdev
                cdev_init(&dev[i]->cdev, &fops);
                dev[i]->cdev.owner = THIS_MODULE;

                // Add to cdev chain
                ret = cdev_add(&dev[i]->cdev, MKDEV(MAJOR(dev_num), i), 1);

                if (ret) {
                        printk("Bad cdev\n");
                        return ret;
                }

                // Register the device driver
                // ToDo: check return value
                s_dev[i] = device_create(s_dev_class, NULL, 
                                        MKDEV(MAJOR(dev_num), i),
                                        NULL, dev[i]->name);
        }

        return 0;
}

static void ht530_exit(void) {
        int i;
        printk(KERN_ALERT "Goodbye, world\n");

        // Destroy devices
        for (i = 0; i < DEVICE_NUMBER; ++i) {
                // Remove device from file system first
                device_destroy(s_dev_class, MKDEV(MAJOR(dev_num), i));
                // Remove from cdev chain
                cdev_del(&dev[i]->cdev);
                // No one can access anymore.
                printk(KERN_ALERT "Removing hash table\n");
                // Destroy hash table
                kfree(dev[i]);
        }

        // Destroy driver_class
        class_destroy(s_dev_class);        

        // Release the major number
        unregister_chrdev_region(dev_num, DEVICE_NUMBER);
}

module_init(ht530_init);
module_exit(ht530_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xiangyu Guo");
MODULE_DESCRIPTION("Assignment 1");