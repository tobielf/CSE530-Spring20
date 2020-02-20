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

#include <pthread.h>

#include <sys/ioctl.h>

#include "common.h"

#define BUFF_SIZE       (1024)
#define NUM_OF_DATA_T   (5)
#define MOD_BASE        (100007)
#define BUCKET_SIZE     (128)

const static char* dev_path[] = {"/dev/rb530_dev1", "/dev/rb530_dev2"};

typedef void(*ops_func)(int, int, int);

static void search(int fd, int base, int offset);
static void addition(int fd, int base, int offset);
static void deletion(int fd, int base, int offset);

static int rand_key(int base, int offset);
static int rand_data();

static void dump(int fd);

ops_func operations[] = {search, addition, deletion};

void *data_thread(void *vargp) {
    int fd;
    int base;
    int offset;
    int i;
    unsigned int sleep_usec;

    srand(time(0));

    // pick a number between 150 and 200.
    offset = (rand() % (200 - 150 + 1)) + 150;

    // use thread id as the base key.
    base = pthread_self() % MOD_BASE;

    printf("%d %d\n", base, offset);

    i = (rand() % 2);
    fd = open(dev_path[i], O_RDWR);
    if (fd < 0)
        exit(ENODEV);

    // populate the tables with 150-200 objects
    for (i = base; i < base + offset; i++) {
        int ret;
        rb_object_t obj;
        obj.key = i;
        obj.data = i;
        // check return
        ret = write(fd, &obj, sizeof(struct rb_object));
        if (ret < 0)
            printf("%s\n", strerror(errno));
        sleep_usec = (rand() % 100);
        usleep(sleep_usec);
    }

    // 100 search, addition, or deletion operations
    for (i = 0; i < 100; i++) {
        int op_code = rand() % 3;
        operations[op_code](fd, base, offset);
        sleep_usec = (rand() % 100);
        usleep(sleep_usec);
    }

    close(fd);

    return NULL; 
}

void *debug_thread(void *vargp) {
    int fd;
    int ret;
    char command[BUFF_SIZE];

    fd = open("/dev/rbprobe", O_RDWR);
    if (fd < 0) {
        printf("Already opened %d\n", errno);
        exit(EBUSY);
    }
    while (scanf("%s", command)!=EOF) {
        if (strcmp("probe", command) == 0) {
            char param[BUFF_SIZE];
            unsigned int offset;
            printf("Input probe offset in heximal, e.g.0x15:\n");
            scanf("%s", param);
            offset = strtoul(param, NULL, 0);
            printf("%x\n", offset);
            write(fd, &offset, sizeof(unsigned int));
        } else if (strcmp("clear", command) == 0) {
            if (ioctl(fd, MP530_CLEAR_PROBES, NULL) == -1) {
                printf("%d\n", errno);
            }
            printf("cleared all probes\n");
        } else if (strcmp("print", command) == 0) {
            mp_info_t info;
            ret = read(fd, &info, sizeof(mp_info_t));
            if (ret < 0) {
                printf("%s\n", strerror(errno));
            } else {
                printf("addr %p, pid %d, rtsc %llu\n", info.addr, info.pid, info.timestamp);
            }
        } else {
            printf("Not support this command\n");
            printf("Supported command \"probe\", \"print\", \"clear\"\n");
        }
    }

    close(fd);
    return NULL;
}

int main(int argc, char const *argv[]) {
    pthread_t tid_data[NUM_OF_DATA_T];
    pthread_attr_t attr[NUM_OF_DATA_T];
    struct sched_param param[NUM_OF_DATA_T];

    pthread_t tid_debug;
    int fd;
    int i;

    printf("Before Thread\n"); 

    for (i = 0; i < NUM_OF_DATA_T; ++i) {
        // attr_init
        pthread_attr_init(&attr[i]);
        param[i].sched_priority = i;
        pthread_attr_setschedparam(&attr[i], &param[i]);
        pthread_create(&tid_data[i], &attr[i], data_thread, NULL); 
    }

    pthread_create(&tid_debug, NULL, debug_thread, NULL);

    for (i = 0; i < NUM_OF_DATA_T; ++i) {
        pthread_join(tid_data[i], NULL);
    }

    printf("After Thread\n");

    for (i = 0; i < 2; i++) {
        fd = open(dev_path[i], O_RDWR);
        if (fd < 0)
            return ENODEV;

        dump(fd);

        close(fd);
    }

    pthread_join(tid_debug, NULL);

    return 0;
}

static void search(int fd, int base, int offset) {
    int ret;
    rb_object_t obj;

    obj.key = rand_key(base, offset);

    ret = read(fd, &obj, sizeof(rb_object_t));
    if (ret < 0)
        printf("%s\n", strerror(errno));
}

static void addition(int fd, int base, int offset) {
    int ret;
    rb_object_t obj;

    obj.key = rand_key(base, offset);
    obj.data = rand_data();

    ret = write(fd, &obj, sizeof(struct rb_object));
    if (ret < 0)
        printf("%s\n", strerror(errno));
}

static void deletion(int fd, int base, int offset) {
    int ret;
    rb_object_t obj;

    obj.key = rand_key(base, offset);
    obj.data = 0;

    ret = write(fd, &obj, sizeof(struct rb_object));
    if (ret < 0)
        printf("%s\n", strerror(errno));
}

static int rand_key(int base, int offset) {
    return (rand() % offset) + base;
}

static int rand_data() {
    return (rand() % MOD_BASE) + 1;
}

static void dump(int fd) {
    int i;
    for (i = 0; i < BUCKET_SIZE; i++) {
        int d;
        d = i;
        if (ioctl(fd, RB530_DUMP_ELEMENTS, &d) == -1) {
            printf("%d\n", errno);
            continue;
        }
    }
}