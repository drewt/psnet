#ifndef _P2P_TCP_H_
#define _P2P_TCP_H_

#include "common.h"

#define TCP_MSG_MAX 100

/* (TCP) connection info */
struct conn_info {
    int sock;
    struct sockaddr_storage addr;
#ifdef P2PSERV_LOG
    char paddr[INET6_ADDRSTRLEN];
#endif
};

/* the number of service threads currently running */
int tcp_threads;
pthread_mutex_t tcp_threads_lock;

int tcp_server_init (char *port);
void __attribute((noreturn)) tcp_server_main (int sockfd, int max_threads,
        void*(*cb)(void*));

size_t tcp_read_message (int sock, char *msg_buf);
size_t tcp_read_bytes (int sock, char *msg_buf, size_t bytes);

#endif
