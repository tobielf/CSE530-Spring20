/**
 * @file ring_buff.h
 */
#ifndef __RING_BUFF_H__
#define __RING_BUFF_H__

typedef struct mp_ring_buff mp_ring_buff_t;
struct mp_ring_buff;

/**
 * @brief create a ring buffer object
 * @return NULL on failed; otherwise a valid pointer to the object.
 */
mp_ring_buff_t *ring_buff_init(void);

/**
 * @brief release a ring buffer object
 * @param obj, a valid ring buffer object.
 */
void ring_buff_fini(mp_ring_buff_t *);

/**
 * @brief put an element to ring buffer
 * @param obj, a valid ring buffer object.
 * @param data, a pointer to the data element.
 * @return 0, on success; otherwise errno.
 */
int ring_buff_put(mp_ring_buff_t *, void *);

/**
 * @brief get an element from ring buffer
 * @param obj, a valid ring buffer object.
 * @param data, a pointer of pointer to the data element.
 * @return 0, on success; otherwise errno.
 * @note the caller should free the data after using it.
 */
int ring_buff_get(mp_ring_buff_t *, void **);

#endif