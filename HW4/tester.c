/**
 * @file main.c
 * @brief User Space Testing Program
 *
 * @author Xiangyu Guo
 */
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

#include <sys/ioctl.h>

#include "common.h"

#define BUFF_SIZE (1024)

typedef struct rb_object {
        int key;                        /** Key of the rbtree object */
        int data;                       /** Data of the rbtree object */
} rb_object_t;

int main(int argc, char const *argv[]) {
    int fd;
    int fd_probe;
    int ret;

    rb_object_t obj;
    obj.key = 0;
    obj.data = 1;

    fd_probe = open("/dev/mprobe", O_RDWR);
    if (fd_probe < 0) {
        printf("mprobe already opened by others %d\n", errno);
    }

    fd = open("/dev/mt530-0", O_RDWR);

    if (fd < 0)
        return ENODEV;

    if (argc < 2)
        return EINVAL;

    if (strcmp("read", argv[1]) == 0) {
        if (argc > 2)
            obj.key = atoi(argv[2]);
        ret = read(fd, &obj, sizeof(rb_object_t));
        if (ret < 0)
            printf("No such data\n");
        printf("%d %d %d\n", ret, obj.key, obj.data);
    } else if (strcmp("write", argv[1]) == 0) {
        if (argc < 4)
            return EINVAL;
        obj.key = atoi(argv[2]);
        obj.data = atoi(argv[3]);
        write(fd, &obj, 16);
    } else if (strcmp("probe", argv[1]) == 0) {
        dump_data_t probe;
        if (argc < 4)
            return EINVAL;
        strcpy(probe.symbol_name, argv[2]);
        probe.mode = strtoul(argv[3], NULL, 0);
        printf("symbol_name: %s\n", probe.symbol_name);
        printf("mode: %d\n", probe.mode);
        write(fd_probe, &probe, sizeof(dump_data_t));
    } else if (strcmp("clear", argv[1]) == 0) {
        if (ioctl(fd_probe, MP530_CLEAR_PROBES, NULL) == -1) {
            printf("%d\n", errno);
        }
        printf("cleared all probes\n");
    }

    close(fd_probe);
    close(fd);

    return 0;
}