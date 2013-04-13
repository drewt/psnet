#ifndef _PSNET_UDP_H_
#define _PSNET_UDP_H_

#include <sys/socket.h>
#include <pthread.h>
#ifdef PSNETLOG
#include <netinet/in.h>
#endif

/* the number of service threads currently running */
int udp_threads;
pthread_mutex_t udp_threads_lock;

void udp_send_msg (const char *msg, size_t len, const struct sockaddr *dst);

int udp_server_init (char *port);
_Noreturn void udp_server_main (int sock, int max_threads, void *(*cb)(void*));

#endif
