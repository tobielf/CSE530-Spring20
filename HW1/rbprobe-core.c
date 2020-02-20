/**
 * @file rbprobe.c
 * @brief debugging tool using kprobe
 * @author Xiangyu Guo
 * @reference http://shell-storm.org/blog/Trace-and-debug-the-Linux-Kernel-functons/kprobe_example.c
 *            https://www.kernel.org/doc/Documentation/kprobes.txt
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/fs.h>

#include <linux/version.h>

#include <linux/uaccess.h>
#include <asm/uaccess.h>

#include <linux/kprobes.h>

#include <linux/rbtree.h>

#include <asm/ptrace.h>
#include <asm/atomic.h>
#include <linux/thread_info.h>

#include "common.h"
#include "rb530_drv.h"
#include "ring_buff.h"

#define DEVICE_NAME "rbprobe"
#define CLASS_NAME "kprobedrv"
#define DEVICE_NUMBER (1)

typedef struct kprobe_list {
        struct kprobe kp;                       /** Kprobe info */
        struct list_head next;                  /** Next node */
} kprobe_list_t;

struct rbprobe_dev {
        struct cdev cdev;                       /** The cdev structure */
        mp_ring_buff_t *ring_buff;              /** Ring buff for debug info*/
};

LIST_HEAD(kprobes_list);                        /** Kprobe list */

static atomic_t available = ATOMIC_INIT(1);
static dev_t dev_num = 0;
static struct class *s_dev_class = NULL;
static struct device *s_dev[DEVICE_NUMBER];
static struct rbprobe_dev dev;

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,19,8)
static __always_inline unsigned long long rdtsc(void)
{
        DECLARE_ARGS(val, low, high);

        asm volatile("rdtsc" : EAX_EDX_RET(val, low, high));

        return EAX_EDX_VAL(val, low, high);
}
#else
#include <asm/msr.h>
#endif

/* kprobe pre_handler: called just before the probed instruction is executed */
static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
        struct thread_info *ti = current_thread_info();
        mp_info_t *info;
        unsigned long bkt;
        int cnt = 0;

        struct file *filp;
        struct rb_node *child;
        struct rb_root *root;
        struct rb_dev *devp;

        info = kmalloc(sizeof(mp_info_t), GFP_KERNEL);
        info->addr = p->addr;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,19,8)
        info->pid = ti->task->pid;
#else
        info->pid = 0;
#endif
        info->timestamp = rdtsc();
        // ToDo: dump objects (local variable)
        bkt = regs->bp;
        if (p->offset >= 0x03)
                bkt = *(unsigned long *)bkt;

        filp = (struct file*)*(unsigned long*)(bkt - 0x08);
        devp = filp->private_data;
        root = &devp->root;

        // printk(KERN_INFO "filp address: %p\n", filp);
        // printk(KERN_INFO "root address: %p\n", root);
        child = rb_first(root);
        while(child && cnt < DUMP_SIZE) {
                struct my_node *stuff;
                stuff = rb_entry(child, struct my_node, next);
                printk(KERN_INFO "Reading %d\n", stuff->data.key);
                info->objects.object_array[cnt].key = stuff->data.key;
                info->objects.object_array[cnt].data = stuff->data.data;
                child = rb_next(child);
                ++cnt;
        }
        info->objects.copied = cnt;

        ring_buff_put(dev.ring_buff, info);

        // printk(KERN_INFO "pre_handler: p->addr = 0x%p, offset = 0x%x,ip = %lx,"
        //                 " flags = 0x%lx time: %lld  \n",
        //         p->addr, p->offset, regs->ip, regs->flags, rdtsc());
        // printk(KERN_INFO "bp = %lx sp = %lx\n", regs->bp, regs->sp);

        // for (bkt = regs->bp; bkt >= kernel_stack_pointer(regs); bkt-=4) {
        //         printk(KERN_INFO "address 0x%lx, value 0x%x\n", bkt, *(int *)bkt);
        // }

        // //printk(KERN_INFO "key: %d, data: %d\n", *(int *)(regs->bp - 0x28), *(int *)(regs->bp - 0x24));
        // printk(KERN_INFO "key: %ld, data: %ld\n", regs->si, regs->dx);

        // printk(KERN_INFO "DI: %lx SI: %lx DX: %lx CX: %lx\n", regs->di, regs->si, regs->dx, regs->cx);
        // printk(KERN_INFO "EBP:%lx ESP:%lx\n", regs->bp, kernel_stack_pointer(regs));
        // stack tracing hackery to print local variable values
        //
        /* A dump_stack() here will give a stack backtrace */
        // dump_stack();
        return 0;
}

/* kprobe post_handler: called after the probed instruction is executed */
static void handler_post(struct kprobe *p, struct pt_regs *regs,
                                unsigned long flags) {
        printk(KERN_INFO "post_handler: p->addr = 0x%p, flags = 0x%lx\n",
                p->addr, regs->flags);
}

/*
 * fault_handler: this is called if an exception is generated for any
 * instruction within the pre- or post-handler, or when Kprobes
 * single-steps the probed instruction.
 */
static int handler_fault(struct kprobe *p, struct pt_regs *regs, int trapnr) {
        printk(KERN_INFO "fault_handler: p->addr = 0x%p, trap #%dn",
                p->addr, trapnr);
        /* Return 0 because we don't handle the fault. */
        return 0;
}

static int rbprobe_open(struct inode *, struct file *);
static int rbprobe_release(struct inode *, struct file *);
static ssize_t rbprobe_read(struct file *, char *, size_t, loff_t *);
static ssize_t rbprobe_write(struct file *, const char *, size_t, loff_t *);
static long rbprobe_ioctl(struct file *, unsigned int cmd, unsigned long arg);

static void rbprobe_remove_all(void);

struct file_operations fops = {
        .owner = THIS_MODULE,
        .open = rbprobe_open,
        .release = rbprobe_release,
        .read = rbprobe_read,
        .write = rbprobe_write,
        .unlocked_ioctl = rbprobe_ioctl        
};

static int rbprobe_open(struct inode *i, struct file *filp) {
        struct rbprobe_dev *devp;

        // Device access control, singleton open 
        if (!atomic_dec_and_test(&available)) {
                atomic_inc(&available);
                return -EBUSY;
        }
        devp = container_of(i->i_cdev, struct rbprobe_dev, cdev);
        filp->private_data = devp;
        return 0;
}

static int rbprobe_release(struct inode *i, struct file *filp) {
        // Release the resource, so it can be opened by other process.
        atomic_inc(&available);
        return 0;
}

static ssize_t rbprobe_read(struct file *filp, char *buf,
                        size_t count, loff_t * off) {
        /** Retrieve the trace data items */
        mp_info_t *info;

        if (ring_buff_get(dev.ring_buff, (void **)&info))
                return -EINVAL;

        if (info == NULL)
                return -EINVAL;

        count = min(count, sizeof(mp_info_t));
        if (count < sizeof(mp_info_t))
                return -EINVAL;

        if (copy_to_user(buf, info, count))
                return -EFAULT;

        kfree(info);

        return 0;
}

static ssize_t rbprobe_write(struct file *filp, const char *buf,
                        size_t count, loff_t * off) {
        /** Register kprobe based on the address getting from the user*/
        int ret;
        rb_probe_t probe;
        kprobe_list_t *obj;

        obj = kmalloc(sizeof(kprobe_list_t), GFP_KERNEL);
        if (obj == NULL)
                return -ENOMEM;

        count = min(sizeof(rb_probe_t), count);
        // get offset from user
        if (copy_from_user(&probe, buf, count))
                return -EFAULT;

        printk(KERN_INFO "OP_code: %d\n", probe.op_code);
        printk(KERN_INFO "Offset: %x\n", probe.offset);

        obj->kp.pre_handler   = handler_pre;
        obj->kp.post_handler  = handler_post;
        obj->kp.fault_handler = handler_fault;
        obj->kp.symbol_name   = func_name[probe.op_code];
        obj->kp.addr = NULL;
        // obj->kp.symbol_name = NULL;
        // obj->kp.addr = (kprobe_opcode_t *) kallsyms_lookup_name("dev_write");
        // Use the "offset" field of struct kprobe
        // if the offset into the symbol to install
        // a probepoint is known. This field is used
        // to calculate the probepoint.
        obj->kp.offset = probe.offset;

        INIT_LIST_HEAD(&obj->next);

        ret = register_kprobe(&obj->kp);
        if (ret < 0) {
                printk(KERN_INFO "register_kprobe failed, returned %d\n", ret);
                kfree(obj);
                return ret;
        }
        printk(KERN_INFO "Planted kprobe at %p\n", obj->kp.addr);

        // add to the hlist
        list_add(&obj->next, &kprobes_list);
        
        return 0;
}

static long rbprobe_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
        // rbprobe_remove_all()
        switch (cmd) {
                case MP530_CLEAR_PROBES:
                        rbprobe_remove_all();
                        break;
                default:
                        return -EINVAL;
        }
        return 0;
}

static void rbprobe_remove_all(void) {
        kprobe_list_t *pos, *tmp;

        list_for_each_entry_safe(pos, tmp, &kprobes_list, next) {
                unregister_kprobe(&pos->kp);
                printk(KERN_INFO "kprobe at %p unregistered\n", pos->kp.addr);
                list_del(&pos->next);
                kfree(pos);
        }
}

static int rbprobe_init(void)
{
        int ret;
        // Get a device number for the driver
        ret = alloc_chrdev_region(&dev_num, 0, DEVICE_NUMBER, 
                                        DEVICE_NAME);

        // Register the device class
        // ToDo: check return value
        s_dev_class = class_create(THIS_MODULE, CLASS_NAME);

        // Init ring buff
        dev.ring_buff = ring_buff_init();

        // Create cdev
        cdev_init(&dev.cdev, &fops);
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

static void rbprobe_exit(void)
{

        // Remove device from file system first
        device_destroy(s_dev_class, MKDEV(MAJOR(dev_num), 0));
        
        // Remove from cdv chain
        cdev_del(&dev.cdev);

        // Clean up ring buff
        ring_buff_fini(dev.ring_buff);

        // Destroy kprobe list rbprobe_remove_all
        rbprobe_remove_all();

        // Destroy driver_class
        class_destroy(s_dev_class);

        // Release the major number
        unregister_chrdev_region(dev_num, DEVICE_NUMBER);
}

module_init(rbprobe_init)
module_exit(rbprobe_exit)
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xiangyu Guo");
MODULE_DESCRIPTION("Assignment 1, rbprobe");