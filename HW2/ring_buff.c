/**
 * @file ring_buff.c
 * @brief a simple implementation of a ring buffer
 * @author Xiangyu Guo
 */
#include <linux/slab.h>

#include "ring_buff.h"

struct mp_ring_buff {
        unsigned int head;              /**< Buffer head */
        unsigned int tail;              /**< Buffer tail */
        unsigned int buff_size;         /**< Buffer size */
        free_func free_fn;              /**< Free function */
        spinlock_t lock;                /**< Buffer lock */
        void **data;                    /**< Ring buffer */
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

mp_ring_buff_t *ring_buff_init(unsigned int buff_size, free_func free_fn) {
        mp_ring_buff_t *obj;
        obj = kmalloc(sizeof(mp_ring_buff_t), GFP_KERNEL);
        if (obj == NULL) {
                printk(KERN_ALERT "No mem\n");
                return NULL;
        }
        // init ring buffer
        obj->head = 0;
        obj->tail = 0;
        obj->buff_size = buff_size + 1;
        obj->data = kmalloc(sizeof(void *) * buff_size, GFP_KERNEL);
        if (obj->data == NULL) {
                kfree(obj);
                return NULL;
        }

        obj->free_fn = free_fn;

        // init lock;
        spin_lock_init(&obj->lock);
        return obj;
}

void ring_buff_fini(mp_ring_buff_t *obj) {
        if (obj == NULL)
                return;

        ring_buff_removeall(obj);
        kfree(obj);
}

int ring_buff_removeall(mp_ring_buff_t *obj) {
        if (obj == NULL)
                return -EINVAL;
        // lock
        spin_lock(&obj->lock);

        ring_buff_removeall_nolock(obj);

        // unlock
        spin_unlock(&obj->lock);
        return 0;
}

int ring_buff_removeall_nolock(mp_ring_buff_t *obj) {
        if (obj == NULL)
                return -EINVAL;

        while (!ring_buff_is_empty(obj)) {
                if (obj->free_fn)
                        obj->free_fn(obj->data[obj->head]);
                obj->head = (obj->head + 1) % obj->buff_size;
        }

        return 0;
}

int ring_buff_put(mp_ring_buff_t *obj, void *data) {
        if (obj == NULL)
                return -EINVAL;

        // lock
        spin_lock(&obj->lock);

        ring_buff_put_nolock(obj, data);

        // unlock
        spin_unlock(&obj->lock);
        return 0;
}

int ring_buff_put_nolock(mp_ring_buff_t *obj, void *data) {
        if (obj == NULL)
                return -EINVAL;

        if (ring_buff_is_full(obj)) {
                // remove the oldest one element
                if (obj->free_fn)
                        obj->free_fn(obj->data[obj->head]);
                obj->head = (obj->head + 1) % obj->buff_size;
        }

        obj->data[obj->tail] = data;
        obj->tail = (obj->tail + 1) % obj->buff_size;

        return 0;
}

int ring_buff_get(mp_ring_buff_t *obj, void **data) {
        int ret;
        if (obj == NULL)
                return -EINVAL;
        // lock
        spin_lock(&obj->lock);

        ret = ring_buff_get_nolock(obj, data);

        // unlock
        spin_unlock(&obj->lock);
        return ret;
}

int ring_buff_get_nolock(mp_ring_buff_t *obj, void **data) {
        if (obj == NULL)
                return -EINVAL;

        if (ring_buff_is_empty(obj))
                return -EINVAL;

        *data = obj->data[obj->head];
        obj->head = (obj->head + 1) % obj->buff_size;

        return 0;
}

static int ring_buff_is_empty(mp_ring_buff_t *obj) {
        return obj->head == obj->tail;
}

static int ring_buff_is_full(mp_ring_buff_t *obj) {
        return (obj->tail + 1) % obj->buff_size == obj->head;
}