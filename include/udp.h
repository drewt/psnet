#ifndef _P2P_UDP_H_
#define _P2P_UDP_H_

#include <sys/socket.h>
#include <pthread.h>
#ifdef P2PSERV_LOG
#include <netinet/in.h>
#endif

#define UDP_MSG_MAX 128

/* message info: source address and message */
struct msg_info {
    struct sockaddr_storage addr;
    size_t len;
    char msg[UDP_MSG_MAX];
#ifdef P2PSERV_LOG
    char paddr[INET6_ADDRSTRLEN];
#endif
};

/* the number of service threads currently running */
int udp_threads;
pthread_mutex_t udp_threads_lock;

void udp_send_msg (const char *msg, size_t len,
        const struct sockaddr_storage *dst);

int udp_server_init (char *port);
void __attribute((noreturn)) udp_server_main (int sockfd, int max_threads,
        void *(*cb)(void*));

#endif
