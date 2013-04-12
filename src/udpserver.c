#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "common.h"
#include "udp.h"

int udp_threads;
pthread_mutex_t udp_threads_lock;

// XXX: doesn't really belong here, but doesn't belong anywhere else either
void udp_send_msg (const char *msg, size_t len, const struct sockaddr *dst)
{
    socklen_t sin_size;
    int s;

    if ((s = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        perror ("socket");
        return;
    }

    sin_size = dst->sa_family == AF_INET ? sizeof (struct sockaddr_in) :
            sizeof (struct sockaddr_in6);

    if (sendto (s, msg, len, 0, dst, sin_size) == -1)
        perror ("sendto");
}

int udp_server_init (char *port)
{
    struct addrinfo hints, *servinfo, *p;
    const int yes = 1;
    int sockfd;
    int rc;

    memset (&hints, 0, sizeof (hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags    = AI_PASSIVE;

    if ((rc = getaddrinfo (NULL, port, &hints, &servinfo))) {
        fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (rc));
        exit (EXIT_FAILURE);
    }

    for (p = servinfo; p; p = p->ai_next) {
        if ((sockfd = socket (p->ai_family, p->ai_socktype,
                        p->ai_protocol)) == -1) {
            perror ("socket");
            continue;
        }

        if (setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                    sizeof (int)) == -1) {
            perror ("setsockopt");
            exit (EXIT_FAILURE);
        }

        if (bind (sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close (sockfd);
            perror ("bind");
            continue;
        }

        break;
    }

    if (!p) {
        fprintf (stderr, "server: failed to bind\n");
        exit (EXIT_FAILURE);
    }

    freeaddrinfo (servinfo);

    udp_threads = 0;
    pthread_mutex_init (&udp_threads_lock, NULL);

    return sockfd;
}

_Noreturn void udp_server_main (int sock, int max_threads, void *(*cb)(void*))
{
    struct msg_info *msg;
    socklen_t sin_size;
    ssize_t rc;
    pthread_t tid;

    for(;;) {
        msg = malloc (sizeof (struct msg_info));
        sin_size = sizeof (struct sockaddr_in);
        if ((rc = recvfrom (sock, msg->msg, UDP_MSG_MAX-1, 0,
                    (struct sockaddr*) &msg->addr, &sin_size)) == -1) {
            perror ("recvfrom");
            continue;
        }
        msg->msg[rc] = '\0';
        msg->len = rc;

        pthread_mutex_lock (&udp_threads_lock);
        if (udp_threads >= max_threads) {
            fprintf (stderr, "thread limit reached: refusing connection\n");
            pthread_mutex_unlock (&udp_threads_lock);
            free (msg);
            continue;
        }

        udp_threads++;
        pthread_mutex_unlock (&udp_threads_lock);

#ifdef P2PSERV_LOG
        inet_ntop (msg->addr.ss_family,
                get_in_addr ((struct sockaddr*) &msg->addr),
                msg->paddr, sizeof msg->paddr);
        printf ("M %s\n", msg->paddr);
#endif

        if (pthread_create (&tid, NULL, cb, msg))
            perror ("pthread_create");
        else if (pthread_detach (tid))
            perror ("pthread_detach");
    }
    close (sock);
}
