/**
 * @file common.h
 * @brief shared structure definition and macro
 */
#ifndef COMMON_H
#define COMMON_H
#include <linux/ioctl.h>

#include "dynamic_dump_stack.h"
#define NAME_SIZE (128)

typedef struct dump_data {
        char symbol_name[NAME_SIZE];
        dumpmode_t mode;
} dump_data_t;

#define MP530_CLEAR_PROBES _IO('m', 1)

#endif