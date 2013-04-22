/* Copyright 2013 Drew Thoreson */

/* This file is part of psnet
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * psnet is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * psnet.  If not, see <http://www.gnu.org/licenses/>
 */

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

    close (s);
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

    num_threads = 0;
    pthread_mutex_init (&num_threads_lock, NULL);

    return sockfd;
}

_Noreturn void udp_server_main (int sock, void *(*cb)(void*))
{
    struct msg_info *msg;
    socklen_t sin_size;
    ssize_t rc;
    pthread_t tid;

    for(;;) {
        msg = malloc (sizeof (struct msg_info));
        sin_size = sizeof (struct sockaddr_in);
        if ((rc = recvfrom (sock, msg->msg, MSG_MAX-1, 0,
                    (struct sockaddr*) &msg->addr, &sin_size)) == -1) {
            perror ("recvfrom");
            continue;
        }
        msg->msg[rc] = '\0';
        msg->len = rc;

        pthread_mutex_lock (&num_threads_lock);
        if (num_threads >= max_threads) {
            fprintf (stderr, "thread limit reached: discarding message\n");
            pthread_mutex_unlock (&num_threads_lock);
            free (msg);
            continue;
        }

        num_threads++;
        pthread_mutex_unlock (&num_threads_lock);

#ifdef PSNETLOG
        inet_ntop (msg->addr.ss_family,
                get_in_addr ((struct sockaddr*) &msg->addr),
                msg->paddr, sizeof msg->paddr);
        printf ("M %s\n", msg->paddr);
#endif

        if (pthread_create (&tid, NULL, cb, msg))
            perror ("pthread_create");
        else
            pthread_detach (tid);
    }
    close (sock);
}
