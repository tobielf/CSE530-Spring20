/**
 * @file common.h
 * @brief shared structure definition and macro
 */
#ifndef COMMON_H
#define COMMON_H
#include <linux/ioctl.h>

#define DUMP_SIZE (10)
/** rbtree table object */
typedef struct rb_object {
        int key;                        /** Key of the rbtree object */
        int data;                       /** Data of the rbtree object */
} rb_object_t;

/** dump structure */
typedef struct dump_arg {
        int n;                                  /** the n-th device (in) or n objects retrieved (out) */
        int copied;                             /** count number of the elements copied */
        rb_object_t object_array[DUMP_SIZE] ;   /** to retrieve at most 10 objects from the n-th device */
} dump_arg_t;

#define RB530_DUMP_ELEMENTS _IOWR('d', 1, dump_arg_t *)

typedef struct mp_debug_info {
        void *addr;                     /** the address of the kprobe */
        pid_t pid;                      /** the pid of the running process */
        unsigned long long timestamp;   /** time stamp (x86 TSC) */
        dump_arg_t objects;             /** all objects hashed to the bucket */
} mp_info_t;

#define MP530_CLEAR_PROBES _IO('m', 1)

#endif