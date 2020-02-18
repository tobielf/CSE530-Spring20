/**
 * @file ring_buff.c
 * @brief a simple implementation of a ring buffer
 * @author Xiangyu Guo
 */
#include <linux/slab.h>

#include "ring_buff.h"

#define BUFF_SIZE (4)                   /**< Adjust buffer size here */

struct mp_ring_buff {
        unsigned int head;              /**< Buffer head */
        unsigned int tail;              /**< Buffer tail */
        spinlock_t lock;                /**< Buffer lock */
        void *data[BUFF_SIZE];          /**< Ring buffer */
};

/**
 * @brief ring buffer is empty or not
 * @param obj, a valid ring buffer object.
 * @return 1, on empty; 0 on non-empty.
 */
static int ring_buff_is_empty(mp_ring_buff_t *);

/**
 * @brief ring buffer is full or not
 * @param obj, a valid ring buffer object.
 * @return 1, on full; 0 on not full.
 */
static int ring_buff_is_full(mp_ring_buff_t *);

mp_ring_buff_t *ring_buff_init() {
        mp_ring_buff_t *obj;
        obj = kmalloc(sizeof(mp_ring_buff_t), GFP_KERNEL);
        if (obj == NULL) {
                printk(KERN_ALERT "No mem\n");
                return NULL;
        }
        // init ring buffer
        obj->head = 0;
        obj->tail = 0;
        // init lock;
        spin_lock_init(&obj->lock);
        return obj;
}

void ring_buff_fini(mp_ring_buff_t *obj) {
        if (obj == NULL)
                return;
        while (!ring_buff_is_empty(obj)) {
                printk(KERN_INFO "Removing %d\n", obj->head);
                kfree(obj->data[obj->head]);
                obj->head = (obj->head + 1) % BUFF_SIZE;
        }
        kfree(obj);
}

int ring_buff_put(mp_ring_buff_t *obj, void *data) {
        // lock
        spin_lock(&obj->lock);
        if (ring_buff_is_full(obj)) {
                // remove the oldest one element
                printk(KERN_ALERT "Buff Full, removing %d\n", obj->head);
                kfree(obj->data[obj->head]);
                obj->head = (obj->head + 1) % BUFF_SIZE;
        }
        printk(KERN_INFO "Inserted at %d\n", obj->tail);
        obj->data[obj->tail] = data;
        obj->tail = (obj->tail + 1) % BUFF_SIZE;
        // unlock
        spin_unlock(&obj->lock);
        return 0;
}

int ring_buff_get(mp_ring_buff_t *obj, void **data) {
        // lock
        spin_lock(&obj->lock);
        if (ring_buff_is_empty(obj)) {
                // unlock
                spin_unlock(&obj->lock);
                return -EINVAL;
        }
        *data = obj->data[obj->head];
        obj->head = (obj->head + 1) % BUFF_SIZE;
        // unlock
        spin_unlock(&obj->lock);
        return 0;
}

static int ring_buff_is_empty(mp_ring_buff_t *obj) {
        return obj->head == obj->tail;
}

static int ring_buff_is_full(mp_ring_buff_t *obj) {
        return (obj->tail + 1) % BUFF_SIZE == obj->head;
}