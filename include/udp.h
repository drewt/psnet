#ifndef _PSNET_UDP_H_
#define _PSNET_UDP_H_

#include <sys/socket.h>
#include <pthread.h>
#ifdef PSNETLOG
#include <netinet/in.h>
#endif

void udp_send_msg (const char *msg, size_t len, const struct sockaddr *dst);

int udp_server_init (char *port);
_Noreturn void udp_server_main (int sock, void *(*cb)(void*));

#endif
