/**
 * @file mprobe.c
 * @brief debugging tool using kprobe
 * @author Xiangyu Guo
 * @reference http://shell-storm.org/blog/Trace-and-debug-the-Linux-Kernel-functons/kprobe_example.c
 */
#include <linux/kernel.h>
#include <linux/slab.h>

#include <linux/kprobes.h>
#include <linux/kallsyms.h>
#include <linux/anon_inodes.h>

#include "dynamic_dump_stack.h"

#define ANON_INODE_NAME "dynamic_dump_stack"

typedef struct kprobe_list {
        struct kprobe kp;                       /** Kprobe info */
        pid_t owner;                            /** Owner PID */
        pid_t parent;                           /** Owner's parent PID */
        dumpmode_t mode;                        /** Dump mode */
} kprobe_list_t;

/* kprobe pre_handler: called just before the probed instruction is executed */
static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
        kprobe_list_t *node;
        // printk(KERN_INFO "kprobe address: %p\n", p);
        node = container_of(p, kprobe_list_t, kp);

        if (node->mode == 0) {
                // Check PID == current;
                if (node->owner == current->pid)
                        dump_stack();
        } else if (node->mode == 1) {
                // Check PID parent.
                if (node->owner == current->pid ||
                    (node->parent == current->tgid && 
                     node->owner != current->tgid  && 
                     node->parent != current->pid)) {
                        dump_stack();
                }
        } else {
                /* A dump_stack() here will give a stack backtrace */
                dump_stack();
        }
        return 0;
}

static void clean_up(struct kprobe_list *p) {
        unregister_kprobe(&p->kp);
        kfree(p);
}

static int dynamic_dump_stack_release(struct inode *, struct file *);

static struct file_operations fops = {
        .owner = THIS_MODULE,
        .release = dynamic_dump_stack_release,      
};

static int dynamic_dump_stack_release(struct inode *i, struct file *filp) {
        kprobe_list_t *obj = filp->private_data;
        printk(KERN_INFO "Removing kprobe at %p\n", obj->kp.addr);
        clean_up(obj);
        return 0;
}

static int do_insdump(const char *symbolname, dumpmode_t mode) {
        int ret;
        kprobe_list_t *obj;
        unsigned long address;

        printk(KERN_INFO "Symbol name: %s\n", symbolname);

        address = kallsyms_lookup_name(symbolname);
        if (!address) {
                printk(KERN_ALERT "Invalid symbol name!\n");
                return -EINVAL;
        }

        obj = kzalloc(sizeof(kprobe_list_t), GFP_KERNEL);
        if (obj == NULL) {
                return -ENOMEM;
        }

        obj->kp.pre_handler   = handler_pre;
        obj->kp.addr = (kprobe_opcode_t *)address;
        // Use the "offset" field of struct kprobe
        // if the offset into the symbol to install
        // a probepoint is known. This field is used
        // to calculate the probepoint.
        obj->kp.offset = 0x00;

        ret = register_kprobe(&obj->kp);
        if (ret < 0) {
                printk(KERN_INFO "register_kprobe failed, returned %d\n", ret);
                goto err_obj;
        }
        printk(KERN_INFO "Planted kprobe at %p\n", obj->kp.addr);

        obj->mode = mode;
        obj->owner = current->pid;
        obj->parent = current->tgid;

        ret = anon_inode_getfd(ANON_INODE_NAME, &fops, (void *)obj, O_CLOEXEC);
        if (ret < 0) {
                printk(KERN_INFO "anon_inode_getfd failed, ret:%d\n", ret);
                unregister_kprobe(&obj->kp);
                goto err_obj;
        }

        return ret;

err_obj:
        kfree(obj);
        return ret;
}

#include <linux/module.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/stddef.h>

#include <linux/version.h>

#include <linux/uaccess.h>
#include <asm/uaccess.h>
#include <asm/ptrace.h>

#include "common.h"

#define DEVICE_NAME "mprobe"
#define CLASS_NAME "kprobedrv"
#define DEVICE_NUMBER (1)

struct mprobe_dev {
        struct cdev cdev;                       /** The cdev structure */
};

static dev_t dev_num = 0;
static struct class *s_dev_class = NULL;
static struct device *s_dev[DEVICE_NUMBER];
static struct mprobe_dev dev;

static int mprobe_open(struct inode *, struct file *);
static int mprobe_release(struct inode *, struct file *);
static ssize_t mprobe_write(struct file *, const char *, size_t, loff_t *);
static long mprobe_ioctl(struct file *, unsigned int cmd, unsigned long arg);

struct file_operations mprobe_fops = {
        .owner = THIS_MODULE,
        .open = mprobe_open,
        .release = mprobe_release,
        .write = mprobe_write,
        .unlocked_ioctl = mprobe_ioctl        
};

static int mprobe_open(struct inode *i, struct file *filp) {
        struct mprobe_dev *devp;

        devp = container_of(i->i_cdev, struct mprobe_dev, cdev);
        filp->private_data = devp;
        return 0;
}

static int mprobe_release(struct inode *i, struct file *filp) {
        return 0;
}

static ssize_t mprobe_write(struct file *filp, const char *buf, 
                        size_t count, loff_t * off) {
        /** Register kprobe based on the address getting from the user*/
        dump_data_t d;

        count = min(sizeof(dump_data_t), count);
        // get offset from user
        if (copy_from_user(&d, buf, count)) {
                return -EFAULT;
        }

        return do_insdump(d.symbol_name, d.mode);
}

static long mprobe_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
        // mprobe_remove_all()
        switch (cmd) {
                case MP530_CLEAR_PROBES:
                        break;
                default:
                        return -EINVAL;
        }
        return 0;
}

static int mprobe_init(void)
{
        int ret;

        // Get a device number for the driver
        ret = alloc_chrdev_region(&dev_num, 0, DEVICE_NUMBER, 
                                        DEVICE_NAME);

        // Register the device class
        // ToDo: check return value
        s_dev_class = class_create(THIS_MODULE, CLASS_NAME);

        // Create cdev
        cdev_init(&dev.cdev, &mprobe_fops);
        dev.cdev.owner = THIS_MODULE;

        // Add to cdev chain
        ret = cdev_add(&dev.cdev, MKDEV(MAJOR(dev_num), 0), 1);
        if (ret) {
                printk("Bad cdev\n");
                return ret;
        }

        // Register the device driver
        // ToDo: check return value
        s_dev[0] = device_create(s_dev_class, NULL,
                                MKDEV(MAJOR(dev_num), 0),
                                NULL, DEVICE_NAME);

        return 0;
}

static void mprobe_exit(void)
{
        // Remove device from file system first
        device_destroy(s_dev_class, MKDEV(MAJOR(dev_num), 0));
        
        // Remove from cdv chain
        cdev_del(&dev.cdev);

        // Destroy driver_class
        class_destroy(s_dev_class);

        // Release the major number
        unregister_chrdev_region(dev_num, DEVICE_NUMBER);
}

module_init(mprobe_init)
module_exit(mprobe_exit)
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xiangyu Guo");
MODULE_DESCRIPTION("Assignment 4, dump_stack");