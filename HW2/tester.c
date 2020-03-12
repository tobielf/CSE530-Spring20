/**
 * @file main.c
 * @brief User Space Testing Program (Command line)
 *
 * @author Xiangyu Guo
 */
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>

#include <sys/ioctl.h>

#include "common.h"

#define BUFF_SIZE (1024)            /**< Default buffer size */
#define MAX_DEVICES (10)            /**< Max device going to support */

static void usage() {
    printf("===============================================================\n");
    printf("|    HCSR Usage:                                              |\n");
    printf("|        read : ./tester <dev> read                           |\n");
    printf("|        write: ./tester <dev> write <integer_value>          |\n");
    printf("|        pins : ./tester <dev> pins <trigger_pin> <echo_pin>  |\n");
    printf("|        param: ./tester <dev> param <m_samples> <delta>      |\n");
    printf("|    More Instructions see README                             |\n");
    printf("|       Contact: Xiangyu.Guo@asu.edu                          |\n");
    printf("===============================================================\n");
}

int main(int argc, char const *argv[]) {
    char path[BUFF_SIZE];
    int fd[MAX_DEVICES];
    int upper = -1;
    int ret;
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

    if (argc < 3) {
        usage();
        return EINVAL;
    }

    idx = atoi(argv[1]);
    if (idx < 0 || idx > 2) {
        printf("Device number out of range.\n");
        return EINVAL;
    }

    if (strcmp("read", argv[2]) == 0) {
        result_info_t r;

        ret = read(fd[idx], &r, sizeof(result_info_t));
        if (ret < 0) {
            printf("No data\n");
            return errno;
        }

        printf("%d %llu %llu\n", ret, r.measurement, r.timestamp);
        printf("Distance %llu(centi-meter)\n", r.measurement);
    } else if (strcmp("write", argv[2]) == 0) {
        int value;
        if (argc < 4) {
            usage();
            return EINVAL;
        }

        value = atoi(argv[3]);
        if (write(fd[idx], &value, sizeof(int))) {
            printf("Write error, %s\n", strerror(errno));
            return errno;
        }
    } else if (strcmp("pins", argv[2]) == 0) {
        pins_setting_t p;
        p.trigger_pin = 10;
        p.echo_pin = 6;

        if (argc < 5) {
            usage();
            return EINVAL;
        }

        p.trigger_pin = atoi(argv[3]);
        p.echo_pin = atoi(argv[4]);

        if (ioctl(fd[idx], CONFIG_PINS, &p)) {
            printf("Setup pins error, %s\n", strerror(errno));
            return errno;
        }
    } else if (strcmp("param", argv[2]) == 0) {
        parameters_setting_t param;
        param.m = 10;
        param.delta = 200;

        if (argc < 5) {
            usage();
            return EINVAL;
        }

        param.m = atoi(argv[3]);
        param.delta = atoi(argv[4]);

        if (ioctl(fd[idx], SET_PARAMETERS, &param)) {
            printf("Setup params error, %s\n", strerror(errno));
            return errno;
        }
    } else if (strcmp("fun", argv[2]) == 0) {
        result_info_t r;

        while (1) {
            ret = read(fd[idx], &r, sizeof(result_info_t));
            if (ret < 0) {
                printf("No data\n");
                return errno;
            }

            printf("Distance %llu(centi-meter)\n", r.measurement);
        }
    } else {
        usage();
    }
    for (idx = 0; idx <= upper; ++idx) {
        close(fd[idx]);
    }

    return 0;
}