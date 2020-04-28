/**
 * @file sys_test.c
 * @brief user space syscall testing
 */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include <pthread.h>

#include "dynamic_dump_stack_lib.h"

#ifndef GRADING
#define TEST_SYMBOL1    "dev_write"                 /**< give a valid symbol name, you can change it*/
#define TEST_SYMBOL2    "dev_read"                  /**< another symbol in the test driver */
#define TEST_SYMBOL3    "invalid_symbol_test"       /**< give an invalid symbol name. */
#define TEST_SYMBOL4    "test_data_section_symbol"  /**< give a symbol name located outside text section */
#endif

#define TEST_DRV        "/dev/mt530-0"

/**
1. Correct insertion and removal
    including valid/invalid symbol addresses
    removing by owner/nonowner processes
    when owner process is terminated.

2. Correct dump_stack operations under different dumpmode.
    a. dumpmode 0, inserted by parent process, dump by itself.
    b. dumpmode 0, inserted by parent process, dump by child process.

    c. dumpmode 1, inserted by parent process, dump by itself. [should print out nothing].

    b. dumpmode 1, inserted by child 1 process, dump by child 2 process.

    c. dumpmode 2, inserted by parent process, dump by child process.
*/

/**
 *
 */
void *case9_thread_0(void *arg) {
    insdump(TEST_SYMBOL2, 1);
    // wait for 10 seconds.
    sleep(10);
    return NULL;
}

/**
 *
 */
void *case9_thread_1(void *arg) {
    int fd;
    int cnt = 0;
    fd = open(TEST_DRV, O_RDWR);
    if (fd < 0) {
        printf("File not exist, insert the driver?\n");
        return NULL;
    }

    while (1) {
        cnt++;
        write(fd, &fd, 1);
        printf("Please check dmesg for result.\n");
        sleep(1);
        if (cnt == 10)
            break;
    }
    return NULL;
}

/**
 *
 */
void *case7_thread(void *arg) {
    // try to dump it.
    int fd = *(int *)arg;

    write(fd, &fd, 0);
    return NULL;
}

/** 
 *
 */
void *case5_thread(void *arg) {
    // dump it and exit.
    printf("Testing on dump and exit.\n");
    insdump(TEST_SYMBOL2, 0);
    printf("Please check dmesg for result.\n");
    return NULL;
}

/** 
 *
 */
void *case4_thread(void *arg) {
    int ret;
    int dumpid = *(int *)arg;

    // try to remove the dump inserted by parent.
    // should fail.
    ret = rmdump(dumpid);
    if (ret < 0) {
        printf("Passed! return: %d\n", ret);
    } else {
        printf("Failed! return: %d\n", ret);
    }

    return NULL;
}

void test_suite_six(void) {
    // 10. dumpmode 2, inserted by parent process, dump by child process.
    int ret = 0;
    int fd;
    pthread_t debug;

    ret = insdump(TEST_SYMBOL1, 2);
    if (ret < 0) {
        printf("Insert dump failed, errno: %d\n", ret);
    } else {
        printf("Insert dump success, dumpid: %d\n", ret);
    }

    fd = open(TEST_DRV, O_RDWR);
    if (fd < 0) {
        printf("File not exist, insert the driver?\n");
        return;
    }

    write(fd, &ret, 1);
    printf("Please check dmesg for result.\n");

    pthread_create(&debug, NULL, case7_thread, (void *)&fd);
    pthread_join(debug, NULL);

    close(fd);
    rmdump(ret);
}

void test_suite_five(void) {
    // 9. dumpmode 1, inserted by child 1 process, dump by child 2 process.
    pthread_t debug[2];

    pthread_create(&debug[0], NULL, case9_thread_0, NULL);
    pthread_create(&debug[1], NULL, case9_thread_1, NULL);

    pthread_join(debug[0], NULL);
    pthread_join(debug[1], NULL);
}

void test_suite_four(void) {
    // 8. dumpmode 1, inserted by parent process, dump by itself. [should print out nothing].
    int ret = 0;
    int fd;

    ret = insdump(TEST_SYMBOL1, 1);
    if (ret < 0) {
        printf("Insert dump failed, errno: %d\n", ret);
    } else {
        printf("Insert dump success, dumpid: %d\n", ret);
    }

    fd = open(TEST_DRV, O_RDWR);
    if (fd < 0) {
        printf("File not exist, insert the driver?\n");
        return;
    }

    write(fd, &ret, 1);
    printf("Please check dmesg for result.\n");

    close(fd);
    rmdump(ret);
}

void test_suite_three(void) {
    // 6. dumpmode 0, inserted by parent process, dump by itself.
    int ret = 0;
    int fd;
    pthread_t debug;

    ret = insdump(TEST_SYMBOL1, 0);
    if (ret < 0) {
        printf("Insert dump failed, errno: %d\n", ret);
    } else {
        printf("Insert dump success, dumpid: %d\n", ret);
    }

    fd = open(TEST_DRV, O_RDWR);
    if (fd < 0) {
        printf("File not exist, insert the driver?\n");
        return;
    }

    write(fd, &ret, 1);
    printf("Please check dmesg for result.\n");

    // 7. dumpmode 0, inserted by parent process, dump by child process.
    pthread_create(&debug, NULL, case7_thread, (void *)&fd);
    pthread_join(debug, NULL);

    close(fd);
    rmdump(ret);
}

void test_suite_two(void) {
    int ret;
    pthread_t debug;
    // 4. insert a valid symbol and remove by non-owner. [thread]
    printf("Testing on insert valid symbol by the owner and remove by the non-owner\n");
    ret = insdump(TEST_SYMBOL1, 0);
    if (ret < 0) {
        printf("Failed! return: %d\n", ret);
    } else {
        printf("Passed! dumpid is: %d\n", ret);
    }
    // pass the dumpid to the thread.
    pthread_create(&debug, NULL, case4_thread, (void *)&ret);
    pthread_join(debug, NULL);

    // 5. insert a valid symbol and exit the program. [thread]
    printf("Testing on insert valid symbol and exit the program\n");
    pthread_create(&debug, NULL, case5_thread, NULL);
    pthread_join(debug, NULL);
}

void test_suite_one(void) {
    int ret;
    // 1. test on invalid symbol address.
    printf("Testing on invalid symbol.\n");
    ret = insdump(TEST_SYMBOL3, 1);
    if (ret < 0) {
        printf("Passed! return: %d\n", ret);
    } else {
        printf("Failed!\n");
    }

    // 2. test on symbol address not on the text section.
    printf("Testing on non-text section symbol.\n");
    ret = insdump(TEST_SYMBOL4, 1);
    if (ret < 0) {
        printf("Passed! return: %d\n", ret);
    } else {
        printf("Failed\n");
    }

    // 3. insert a valid symbol and remove by the owner.
    printf("Testing on insert and remove a valid symbol by the owner\n");
    ret = insdump(TEST_SYMBOL1, 2);
    if (ret < 0) {
        printf("Failed! return: %d\n", ret);
    } else {
        printf("Passed! dumpid is: %d\n", ret);
    }
    ret = rmdump(ret);
    if (ret < 0) {
        printf("Failed! return: %d\n", ret);
    } else {
        printf("Passed!\n");
    }
}

int main(int argc, char const *argv[])
{
    test_suite_one();
    test_suite_two();
    test_suite_three();
    test_suite_four();
    test_suite_five();
    test_suite_six();

    return 0;
}