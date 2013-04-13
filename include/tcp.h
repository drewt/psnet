#ifndef _PSNET_TCP_H_
#define _PSNET_TCP_H_

#include <sys/socket.h>
#include <pthread.h>
#ifdef PSNETLOG
#include <netinet/in.h>
#endif

int tcp_server_init (char *port);
_Noreturn void tcp_server_main (int sock, void*(*cb)(void*));

size_t tcp_read_message (int sock, char *msg_buf);
size_t tcp_read_bytes (int sock, char *msg_buf, size_t bytes);

#endif
