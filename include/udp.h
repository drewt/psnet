#ifndef _PSNET_UDP_H_
#define _PSNET_UDP_H_

#include <sys/socket.h>
#include <pthread.h>
#ifdef P2PSERV_LOG
#include <netinet/in.h>
#endif

#define UDP_MSG_MAX 512

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
_Noreturn void udp_server_main (int sock, int max_threads, void *(*cb)(void*));

#endif
