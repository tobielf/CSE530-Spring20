/**
 * @file dynamic_dump_stack_lib.h
 * @brief user space syscall wrapper.
 */
#ifndef __DYNAMIC_DUMP_STACK_LIB_H__
#define __DYNAMIC_DUMP_STACK_LIB_H__

#include <sys/syscall.h>
#include <unistd.h>

typedef unsigned int dumpmode_t;

#define __NR_insdump 359        /**< insdump system call id */
#define __NR_rmdump 360         /**< rmdump system call id */

/**
 * @brief insdump, insert a dump point on symbol name.
 * @param symbolname, a symbolname you are going to dump.
 * @param mode, mode you want. [0, 1, other]
 * @note for 0, the dump info only access by the owner
 *       for 1, the dump info access by those processes who has the same parent
 *       for >2,the dump info can be accessed by an process. 
 * @return a valid dumpid on suceess, otherwise errno.
 */
long insdump(const char *symbolname, dumpmode_t mode) {
    return syscall(__NR_insdump, symbolname, mode);
}

/**
 * @brief rmdump, remove a dump point according the dumpid.
 * @param id, a valid dumpid.
 * @return 0 on success, -EINVAL on fail.
 */
long rmdump(unsigned int id) {
    return syscall(__NR_rmdump, id);
}

#endif