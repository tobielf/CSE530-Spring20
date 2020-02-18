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

#include <linux/rbtree.h>

#define DEVICE_NAME_PREFIX "rb530-"
#define CLASS_NAME "chardrv"
#define DEVICE_NUMBER (2)
#define BUFF_SIZE (16)
#define HASH_TABLE_SIZE_BITS (7)

typedef struct rb_node rb_node_t;
typedef struct rb_root rb_root_t;

typedef rb_node_t *(*seek_func)(const rb_root_t *);
typedef rb_node_t *(*move_func)(const rb_node_t *);

seek_func rb_seek[] = {rb_first, rb_last};
move_func rb_move[] = {rb_next , rb_prev};

/** hash node structure */
typedef struct my_node {
        rb_object_t data;               /**< Hash node data object */
        struct rb_node next;            /**< Next pointer for collision */
} my_node_t;

/** per device structure */
struct rb_dev {
        struct cdev cdev;                       /**< The cdev structure */
        char name[BUFF_SIZE];                   /**< Name of the device */
        struct rb_root root;
        struct rb_node *cursor;
        int read_dir;                 
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

static struct my_node *my_rb_search(struct rb_root *root, int value) {
        struct rb_node *node = root->rb_node;

        while (node) {
                struct my_node *stuff = rb_entry(node, struct my_node, next);

                if (stuff->data.key > value)
                        node = node->rb_left;
                else if (stuff->data.key < value)
                        node = node->rb_right;
                else
                        return stuff;
        }

        return NULL;
}

static void my_rb_insert(struct rb_root *root, struct my_node *new) {
        struct rb_node **link = &root->rb_node;
        struct rb_node *parent = NULL;
        int value = new->data.key;

        while(*link) {
                struct my_node *stuff;
                parent = *link;
                stuff = rb_entry(parent, struct my_node, next);

                if (stuff->data.key > value)
                        link = &parent->rb_left;
                else
                        link = &parent->rb_right;
        }

        rb_link_node(&new->next, parent, link);
        rb_insert_color(&new->next, root);
}

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
        struct rb_dev *devp = filp->private_data;
        struct rb_node *cursor = devp->cursor;

        // Security: comparing the count with sizeof(obj), take the min one.
        count = min(count, sizeof(rb_object_t));
        // Check return value
        if (copy_from_user(&obj, buf, count))
                return -EFAULT;

        if (cursor == NULL)
                return -EINVAL;

        obj.key = rb_entry(cursor, struct my_node, next)->data.key;
        obj.data = rb_entry(cursor, struct my_node, next)->data.data;
        // Check return value
        // In both cases, the return value is the amount of memory still 
        // to be copied. The code looks for this error return, and
        // returns -EFAULT to the user if it’s not 0.
        if (copy_to_user(buf, &obj, count))
                return -EFAULT;

        devp->cursor = rb_move[devp->read_dir](cursor);

        return count;
}

static ssize_t dev_write(struct file *filp, const char *buf,
                size_t count, loff_t *ppos) {
        volatile int key, data;
        rb_object_t obj;
        my_node_t *cur;
        // struct hlist_node * tmp;
        struct rb_dev *devp = filp->private_data;
        struct rb_root *root = &devp->root;

        // Security: comparing the count with sizeof(obj), take the min one.
        count = min(count, sizeof(rb_object_t));
        // Check return value
        if (copy_from_user(&obj, buf, count))
                return -EFAULT;

        key = obj.key;
        data = obj.data;

        printk("Object received %d %d \n", obj.key, obj.data);

        cur = my_rb_search(root, key);
        // delete operation
        if (obj.data == 0) {
                if (cur != NULL) {
                        if (&cur->next == devp->cursor)
                                devp->cursor = rb_move[devp->read_dir](devp->cursor);
                        rb_erase(&cur->next, root);
                        kfree(cur);
                }
        } else {
                // add/update operation
                // update before add
                if (cur != NULL) {
                        cur->data.key = key;
                        cur->data.data = data;
                } else {
                        cur = kmalloc(sizeof(my_node_t), GFP_KERNEL);
                        if (cur == NULL)
                                return -ENOMEM;

                        cur->data.key = key;
                        cur->data.data = data;
                        my_rb_insert(root, cur);
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
        // struct hlist_node * tmp;
        // my_node_t *cur;
        int d;

        switch (cmd) {
                case RB530_DUMP_ELEMENTS:
                        if (copy_from_user(&d, (int *)arg, sizeof(int)))
                                return -EFAULT;
                        if ( (d != ASC_ORDER) && (d != DES_ORDER) )
                                return -EINVAL;

                        devp->read_dir = d;
                        devp->cursor = rb_seek[d](&devp->root);
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

                // // Create hash table
                // hash_init(dev[i]->tbl);
                // Create rbtree
                dev[i]->root.rb_node = NULL;
                dev[i]->read_dir = 0;
                dev[i]->cursor = NULL;
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

        struct rb_node *child;
        struct rb_node *parent;
        struct rb_root *root;
        printk(KERN_ALERT "Goodbye, world\n");

        // Destroy devices
        for (i = 0; i < DEVICE_NUMBER; ++i) {
                // Remove device from file system first
                device_destroy(s_dev_class, MKDEV(MAJOR(dev_num), i));
                // Remove from cdev chain
                cdev_del(&dev[i]->cdev);
                // No one can access anymore.
                printk(KERN_ALERT "Removing hash table\n");
                // Destroy rbtree
                root = &dev[i]->root;
                child = rb_first(root);
                parent = NULL;

                while(child) {
                        struct my_node *stuff;
                        parent = child;
                        child = rb_next(parent);
                        stuff = rb_entry(parent, struct my_node, next);
                        printk(KERN_INFO "Removing %d\n", stuff->data.key);
                        kfree(stuff);
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