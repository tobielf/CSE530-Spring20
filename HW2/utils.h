/**
 * @file utils.h
 * @brief Utility functions
 * @author Xiangyu Guo
 */
#ifndef __UTILS_H__
#define __UTILS_H__

/**
 * @brief get tsc timestamp.
 * @return a tsc timestamp in unsigned long
 */
static __always_inline unsigned long long rdtsc(void)
{
        DECLARE_ARGS(val, low, high);

        asm volatile("rdtsc" : EAX_EDX_RET(val, low, high));

        return EAX_EDX_VAL(val, low, high);
}

#endif