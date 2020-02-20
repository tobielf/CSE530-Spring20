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

int main(int argc, char const *argv[]) {
    int fd;
    int fd_probe;
    int ret;

    rb_object_t obj;
    obj.key = 0;
    obj.data = 1;

    fd_probe = open("/dev/rbprobe", O_RDWR);
    if (fd_probe < 0) {
        printf("mprobe already opened by others %d\n", errno);
    }

    fd = open("/dev/rb530_dev1", O_RDWR);

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
    } else if (strcmp("dump", argv[1]) == 0) {
        if (argc < 3)
            return EINVAL;
        int d;
        d = atoi(argv[2]);
        if (ioctl(fd, RB530_DUMP_ELEMENTS, &d) == -1) {
            printf("%d\n", errno);
        }
    } else if (strcmp("probe", argv[1]) == 0) {
        rb_probe_t probe;
        if (argc < 4)
            return EINVAL;
        probe.op_code = strtoul(argv[2], NULL, 0);
        probe.offset = strtoul(argv[3], NULL, 0);
        printf("op_code: %d\n", probe.op_code);
        printf("offset: %x\n", probe.offset);
        write(fd_probe, &probe, sizeof(rb_probe_t));
    } else if (strcmp("clear", argv[1]) == 0) {
        if (ioctl(fd_probe, MP530_CLEAR_PROBES, NULL) == -1) {
            printf("%d\n", errno);
        }
        printf("cleared all probes\n");
    } else if (strcmp("print", argv[1]) == 0) {
        mp_info_t info;
        ret = read(fd_probe, &info, sizeof(mp_info_t));
        if (ret < 0) {
            printf("%s\n", strerror(errno));
            return EINVAL;
        }
        printf("addr %p, pid %d, rtsc %llu\n", info.addr, info.pid, info.timestamp);
        while (info.objects.copied > 0) {
            printf("Key %d, Data %d\n",
                    info.objects.object_array[info.objects.copied - 1].key,
                    info.objects.object_array[info.objects.copied - 1].data);
            info.objects.copied--;
        }
    }

    close(fd_probe);
    close(fd);

    return 0;
}