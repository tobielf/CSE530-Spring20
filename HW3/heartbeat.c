/**
 * @file heartbeat.c
 * @author Xiangyu Guo
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <errno.h>

#include <pthread.h>
#include <stdatomic.h>

#include <netlink/msg.h>
#include <netlink/attr.h>

#include "common.h"

#ifndef GRADING
#define MAX7219_CS_PIN (8)
#define HCSR04_TRIGGER_PIN (7)
#define HCSR04_ECHO_PIN (6)
#endif

#define MS_SCALE    (1000)          /**< 1 Milli-seconds */

static pthread_t dis_measure;       /**< A thread to measure the distance */

volatile atomic_int distance = 100; /**< atomic variable for IPC */

static int send_msg_to_kernel(struct nl_sock *sock, char *message, int type, int length) {
    struct nl_msg* msg;
    int family_id, err = 0;

    family_id = genl_ctrl_resolve(sock, GENL_HB_FAMILY_NAME);
    if(family_id < 0){
        fprintf(stderr, "Unable to resolve family name!\n");
        exit(EXIT_FAILURE);
    }

    msg = nlmsg_alloc();
    if (!msg) {
        fprintf(stderr, "failed to allocate netlink message\n");
        exit(EXIT_FAILURE);
    }

    if(!genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, family_id, 0, 
        NLM_F_REQUEST, type, 0)) {
        fprintf(stderr, "failed to put nl hdr!\n");
        err = -ENOMEM;
        goto out;
    }

    err = nla_put(msg, GENL_HB_ATTR_MSG, length, message);
    if (err) {
        fprintf(stderr, "failed to put nl string!\n");
        goto out;
    }

    err = nl_send_auto(sock, msg);
    if (err < 0) {
        fprintf(stderr, "failed to send nl message!\n");
    }

out:
    nlmsg_free(msg);
    return err;
}

static int skip_seq_check(struct nl_msg *msg, void *arg) {
    return NL_OK;
}

static int on_distance(struct nl_msg *msg, void* arg) {
    struct nlattr *attr[GENL_HB_ATTR_MAX+1];

    genlmsg_parse(nlmsg_hdr(msg), 0, attr, 
            GENL_HB_ATTR_MAX, genl_hb_policy);

    if (!attr[GENL_HB_ATTR_DIS]) {
        fprintf(stdout, "Kernel sent empty message!!\n");
        return NL_OK;
    }

    fprintf(stdout, "Kernel says: %llu \n", 
        nla_get_u64(attr[GENL_HB_ATTR_DIS]));

    distance = nla_get_u64(attr[GENL_HB_ATTR_DIS]);

    return NL_OK;
}

static void prep_nl_sock(struct nl_sock** nlsock) {
    int family_id;
    
    *nlsock = nl_socket_alloc();
    if(!*nlsock) {
        fprintf(stderr, "Unable to alloc nl socket!\n");
        exit(EXIT_FAILURE);
    }

    /* disable seq checks on multicast sockets */
    nl_socket_disable_seq_check(*nlsock);
    nl_socket_disable_auto_ack(*nlsock);

    /* connect to genl */
    if (genl_connect(*nlsock)) {
        fprintf(stderr, "Unable to connect to genl!\n");
        goto exit_err;
    }

    /* resolve the generic nl family id*/
    family_id = genl_ctrl_resolve(*nlsock, GENL_HB_FAMILY_NAME);
    if(family_id < 0){
        fprintf(stderr, "Unable to resolve family name!\n");
        goto exit_err;
    }

    return;

exit_err:
    nl_socket_free(*nlsock); // this call closes the socket as well
    exit(EXIT_FAILURE);
}

void *dis_thread(void *vargp) {
    struct nl_sock* nlsock = NULL;
    struct nl_cb *cb = NULL;
    int ret;
    char message[GENL_HB_ATTR_MSG_MAX];

    prep_nl_sock(&nlsock);

    /* preparing for the respond callback */
    cb = nl_cb_alloc(NL_CB_DEFAULT);
    nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, skip_seq_check, NULL);
    nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, on_distance, NULL);
    do {
        send_msg_to_kernel(nlsock, message, GENL_HB_MEASURE_MSG, 8);
        ret = nl_recvmsgs(nlsock, cb);
        usleep(100 * MS_SCALE);
    } while (!ret);

    nl_cb_put(cb);
    nl_socket_free(nlsock);

    return NULL;
}

int main(int argc, char *argv[]) {
    struct nl_sock* nlsock = NULL;
    char message[GENL_HB_ATTR_MSG_MAX];
    int sleep_time = 100;
    uint8_t big_heart[8] = {0x1c, 0x22, 0x41, 0x82, 0x82, 0x41, 0x22, 0x1c};
    uint8_t small_heart[8] = {0x00, 0x1c, 0x22, 0x44, 0x44, 0x22, 0x1c, 0x00};
    genl_hb_pins_t ps;

    if (argc < 4) {
        printf("Usage: ./heart chip_select trigger echo\n");
        return -EINVAL;
    }

    prep_nl_sock(&nlsock);

    ps.chip_select = atoi(argv[1]);
    ps.trigger = atoi(argv[2]);
    ps.echo = atoi(argv[3]);

    memcpy(message, &ps, sizeof(genl_hb_pins_t));
    message[sizeof(genl_hb_pins_t)] = '\0';

    printf("Trigger:%d, Echo:%d\n", ps.trigger, ps.echo);
    printf("CS:%d, Trigger:%d, Echo:%d\n", MAX7219_CS_PIN, HCSR04_TRIGGER_PIN, HCSR04_ECHO_PIN);

    send_msg_to_kernel(nlsock, message, GENL_HB_CONFIG_MSG, sizeof(genl_hb_pins_t));

    pthread_create(&dis_measure, NULL, dis_thread, NULL);

    while (1) {
        sleep_time = distance * 10 * MS_SCALE;
        if (sleep_time < 500 * MS_SCALE) {
            sleep_time = 500 * MS_SCALE;
        } else if (sleep_time > 2000 * MS_SCALE) {
            sleep_time = 2000 * MS_SCALE;
        }
        printf("Updated sleep time:%d\n", sleep_time);

        memcpy(message, big_heart, 8 * sizeof(uint8_t));
        send_msg_to_kernel(nlsock, message, GENL_HB_DISPLAY_MSG, sizeof(genl_hb_pattern_t));
    
        usleep(sleep_time);
    
        memcpy(message, small_heart, 8 * sizeof(uint8_t));
        send_msg_to_kernel(nlsock, message, GENL_HB_DISPLAY_MSG, sizeof(genl_hb_pattern_t));

        usleep(sleep_time);
    }

    nl_socket_free(nlsock);

    pthread_join(dis_measure, NULL);

    return 0;
}
