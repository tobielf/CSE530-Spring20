/**
 * @file main.c
 * @brief User Space Testing Program (Automated)
 *
 * @author Xiangyu Guo
 */
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <sys/ioctl.h>

#include "common.h"

#define BUFF_SIZE (1024)                /**< Default buffer size */
#define MAX_DEVICES (10)                /**< Maximum device supported */

static char path[BUFF_SIZE];            /**< Device path variable */
static int fd[MAX_DEVICES];             /**< Device file descriptor */
static int upper = -1;                  /**< Available devices upper bound */

void test_suites_one(int idx) {
    result_info_t r;
    pins_setting_t p;
    int ret;

    printf("Testing on invalid read....");
    ret = read(fd[idx], &r, sizeof(result_info_t));
    if (ret < 0) {
        printf("Passed!\n");
    } else {
        printf("Failed!\n");
    }

    p.trigger_pin = 10;
    p.echo_pin = 6;

    printf("Testing on pins setup......");
    if (ioctl(fd[idx], CONFIG_PINS, &p)) {
        printf("Failed!\nSetup pins error, %s\n", strerror(errno));
    } else {
        printf("Passed!\n");
    }
}

void test_suites_two(int idx) {
    result_info_t r;
    int ret;
    int i;

    printf("Testing on read from empty buffer....");
    ret = read(fd[idx], &r, sizeof(result_info_t));
    if (ret < 0) {
        printf("Failed!\n");
    } else {
        printf("Passed!\n");
    }
    printf("Distance %llu(centi-meter)\n", r.measurement);

    printf("Testing on write more than 5 times....\n");
    for (i = 0; i < 6; ++i) {
        int value = 0;

        if (write(fd[idx], &value, sizeof(int))) {
            printf("Write error, %s\n", strerror(errno));
        } else {
            printf("Writing ... %d\n", i);
        }
        sleep(1);
    }

    printf("Testing on read from full buffer....");
    ret = read(fd[idx], &r, sizeof(result_info_t));
    if (ret < 0) {
        printf("Failed!\n");
    } else {
        printf("Passed!\n");
    }
    printf("Distance %llu(centi-meter)\n", r.measurement);

    printf("Testing on write more than 5 times with non-zero value....\n");
    for (i = 0; i < 6; ++i) {
        int value = 1;

        if (write(fd[idx], &value, sizeof(int))) {
            printf("Write error, %s\n", strerror(errno));
        } else {
            printf("Writing ... %d\n", i);
        }
        sleep(1);
    }

    printf("Testing on read from buffer with only one element....");
    ret = read(fd[idx], &r, sizeof(result_info_t));
    if (ret < 0) {
        printf("Failed!\n");
    } else {
        printf("Passed!\n");
    }

    printf("Distance %llu(centi-meter)\n", r.measurement);
}

void test_suites_three(int idx) {
    int ret;
    int value;
    result_info_t r;
    parameters_setting_t param;
    param.m = 10;
    param.delta = 59;

    printf("Testing on set a delta less than 60ms.....\n");
    if (ioctl(fd[idx], SET_PARAMETERS, &param)) {
        printf("Passed!\n");
        printf("Setup params error, %s\n", strerror(errno));
    } else {
        printf("Failed!\n");
    }

    param.delta = 2000;
    printf("Testing on set a delta to 2000ms.....\n");
    if (ioctl(fd[idx], SET_PARAMETERS, &param)) {
        printf("Failed!\n");
        printf("Setup params error, %s\n", strerror(errno));
    } else {
        printf("Passed!\n");
    }

    printf("Testing on write with a long delay....\n");
    if (write(fd[idx], &value, sizeof(int))) {
        printf("Failed!\n");
        printf("Write error, %s\n", strerror(errno));
    } else {
        printf("Passed!\n");
        printf("Job started...\n");
    }

    if (write(fd[idx], &value, sizeof(int))) {
        printf("Passed!\n");
        printf("Write error, %s\n", strerror(errno));
    } else {
        printf("Failed!\n");
    }

    printf("Testing on read from a blocked I/O....");
    ret = read(fd[idx], &r, sizeof(result_info_t));
    if (ret < 0) {
        printf("Failed!\n");
    } else {
        printf("Passed!\n");
    }
    printf("Distance %llu(centi-meter)\n", r.measurement);
}

int main(int argc, char const *argv[])
{
    // Open all devices
    int idx;

    for (idx = 0; idx < MAX_DEVICES; ++idx) {
        snprintf(path, BUFF_SIZE - 1, "/dev/HCSR_%d", idx);
        fd[idx] = open(path, O_RDWR);
        if (fd[idx] < 0) {
            printf("Number of HCSR devices detected: %d\n", upper + 1);
            break;
        }
        upper = idx;
    }

    // Test suites one.
    test_suites_one(0);
    // Test suites two.
    test_suites_two(0);
    // Test suites three.
    test_suites_three(0);

    for (idx = 0; idx <= upper; ++idx) {
        close(fd[idx]);
    }

    return 0;
}