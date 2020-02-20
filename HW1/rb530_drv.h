/**
 * @file rb530_drv.h
 * @brief shared structure definition and macro for driver
 */
#ifndef __RB530_DRV_H__
#define __RB530_DRV_H__
#include <linux/cdev.h>

#include <linux/rbtree.h>

#define BUFF_SIZE (16)

/** rbtree node structure */
typedef struct my_node {
        rb_object_t data;               /**< Data object */
        struct rb_node next;            /**< Tree node */
} my_node_t;

/** per device structure */
struct rb_dev {
        struct cdev cdev;                       /**< The cdev structure */
        char name[BUFF_SIZE];                   /**< Name of the device */
        struct rb_root root;                    /**< Tree root */
        struct rb_node *cursor;                 /**< Reading cursor */
        int read_dir;                           /**< Reading direction */
};

#endif