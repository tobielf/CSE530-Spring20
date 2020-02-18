/**
 * @file rb530_drv.c
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

#include <linux/uaccess.h>
#include <asm/uaccess.h>

#include "common.h"

#include <linux/hashtable.h>

#define DEVICE_NAME_PREFIX "rb530-"
#define CLASS_NAME "chardrv"
#define DEVICE_NUMBER (2)
#define BUFF_SIZE (16)
#define HASH_TABLE_SIZE_BITS (7)

/** hash node structure */
typedef struct my_node {
        rb_object_t data;               /**< Hash node data object */
        struct hlist_node next;         /**< Next pointer for collision */
} my_node_t;

/** per device structure */
struct rb_dev {
        struct cdev cdev;                       /**< The cdev structure */
        char name[BUFF_SIZE];                   /**< Name of the device */
        DECLARE_HASHTABLE(tbl, HASH_TABLE_SIZE_BITS);   /** Hash table */
};

static dev_t dev_num = 0;                       /**< Driver Major Number */
static struct class *s_dev_class = NULL;        /**< Driver Class */
static struct device *s_dev[DEVICE_NUMBER];     /**< FS device nodes */
static struct rb_dev *dev[DEVICE_NUMBER];       /**< Per device objects */

static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);
static long dev_ioctl(struct file *, unsigned int cmd, unsigned long arg);

struct file_operations fops = {
        .owner = THIS_MODULE,
        .open = dev_open,
        .release = dev_release,
        .read = dev_read,
        .write = dev_write,
        .unlocked_ioctl = dev_ioctl
};

static int dev_open(struct inode *i, struct file *filp) {
        struct rb_dev *devp;

        devp = container_of(i->i_cdev, struct rb_dev, cdev);

        filp->private_data = devp;
        return 0;
}

static int dev_release(struct inode *i, struct file *filp) {
        struct rb_dev *devp = filp->private_data;

        printk(KERN_INFO "\n%s is closing\n", devp->name);
        return 0;
}

// Both the read and write methods return a negative value if an
// error occurs. A return value greater than or equal to 0, instead,
// tells the calling program how many bytes have been successfully transferred.
static ssize_t dev_read(struct file *filp, char *buf,
                        size_t count, loff_t *ppos) {
        rb_object_t obj;
        my_node_t *cur;
        struct hlist_node *tmp;
        struct rb_dev *devp = filp->private_data;
        int found = 0;

        // Security: comparing the count with sizeof(obj), take the min one.
        count = min(count, sizeof(rb_object_t));
        // Check return value
        if (copy_from_user(&obj, buf, count))
                return -EFAULT;

        hash_for_each_possible_safe(devp->tbl, cur, tmp, next, obj.key) {
                if (cur->data.key == obj.key) {
                        printk(KERN_ALERT "Found Item\n");
                        obj.data = cur->data.data;
                        found = 1;
                }
        }

        if (!found)
                return -EINVAL;
        // Check return value
        // In both cases, the return value is the amount of memory still 
        // to be copied. The code looks for this error return, and
        // returns -EFAULT to the user if it’s not 0.
        if (copy_to_user(buf, &obj, count))
                return -EFAULT;

        return count;
}

static ssize_t dev_write(struct file *filp, const char *buf,
                size_t count, loff_t *ppos) {
        volatile int key, data;
        rb_object_t obj;
        my_node_t *cur;
        struct hlist_node * tmp;
        struct rb_dev *devp = filp->private_data;

        // Security: comparing the count with sizeof(obj), take the min one.
        count = min(count, sizeof(rb_object_t));
        // Check return value
        if (copy_from_user(&obj, buf, count))
                return -EFAULT;

        key = obj.key;
        data = obj.data;

        printk("Object received %d %d \n", obj.key, obj.data);

        // delete operation
        if (obj.data == 0) {
                hash_for_each_possible_safe(devp->tbl, cur, tmp, next, obj.key) {
                        if (cur->data.key == obj.key) {
                                printk(KERN_ALERT "Deleted Item\n");
                                hash_del(&cur->next);
                                kfree(cur);
                        }
                }
        } else {
                my_node_t *node;
                int found = 0;
                // add/update operation
                // update before add
                hash_for_each_possible_safe(devp->tbl, cur, tmp, next, obj.key) {
                        if (cur->data.key == obj.key) {
                                printk(KERN_ALERT "Duplicated Item\n");
                                cur->data.data = obj.data;
                                found = 1;
                        }
                }
                if (!found) {
                        node = kmalloc(sizeof(my_node_t), GFP_KERNEL);
                        if (node == NULL)
                                return -ENOMEM;

                        node->data.key = obj.key;
                        node->data.data = obj.data;
                        hash_add_rcu(devp->tbl, &node->next, node->data.key);
                }
        }

        // debug purpose
        printk(KERN_INFO "%p", &filp);
        // hash_for_each(devp->tbl, bkt, cur, next) {
        //         printk(KERN_INFO "data=%d is in bucket %d\n", cur->data.data, bkt);
        // }
        
        return 0;
}

static long dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
        struct rb_dev *devp = filp->private_data;
        struct hlist_node * tmp;
        my_node_t *cur;
        dump_arg_t d;
        int cnt = 0;

        switch (cmd) {
                case RB530_DUMP_ELEMENTS:
                        if (copy_from_user(&d, (dump_arg_t *)arg, sizeof(dump_arg_t)))
                                return -EFAULT;
                        if (d.n >= HASH_SIZE(devp->tbl))
                                return -EINVAL;
                        hlist_for_each_entry_safe(cur, tmp, &devp->tbl[d.n], next) {
                                if (cnt < DUMP_SIZE) {
                                        d.object_array[cnt].key = cur->data.key;
                                        d.object_array[cnt].data = cur->data.data;
                                        cnt++;
                                }
                        }
                        d.copied = cnt;
                        if (copy_to_user((dump_arg_t *)arg, &d, sizeof(dump_arg_t)))
                                return -EFAULT;
                        break;
                default:
                        return -EINVAL;
        }
        return 0;
}

static int rb530_init(void) {
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
                dev[i] = kmalloc(sizeof(struct rb_dev), GFP_KERNEL);
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

static void rb530_exit(void) {
        int i;
        my_node_t *obj;
        printk(KERN_ALERT "Goodbye, world\n");

        // Destroy devices
        for (i = 0; i < DEVICE_NUMBER; ++i) {
                int bkt;
                // Remove device from file system first
                device_destroy(s_dev_class, MKDEV(MAJOR(dev_num), i));
                // Remove from cdev chain
                cdev_del(&dev[i]->cdev);
                // No one can access anymore.
                printk(KERN_ALERT "Removing hash table\n");
                // Destroy hash table
                hash_for_each(dev[i]->tbl, bkt, obj, next) {
                        printk(KERN_INFO "Delete %d %d\n", obj->data.key, obj->data.data);
                        hash_del(&obj->next);
                        kfree(obj);
                }
                kfree(dev[i]);
        }

        // Destroy driver_class
        class_destroy(s_dev_class);        

        // Release the major number
        unregister_chrdev_region(dev_num, DEVICE_NUMBER);
}

module_init(rb530_init);
module_exit(rb530_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xiangyu Guo");
MODULE_DESCRIPTION("Assignment 1");