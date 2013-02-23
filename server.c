/* mtserver.c : waits for connections and spawns threads to service them
 *
 * Author: Drew Thoreson
 * Based largely on code from beej's guide
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <pthread.h>

#include "common.h"
#include "ctable.h"

#define BACKLOG 10

int num_threads;
pthread_mutex_t num_threads_lock;

static void usage (void) {
    puts ("usage: mtserver [nclients] [port]\n"
          "\twhere 'nclients' is the maximum number of clients\n"
          "\tand 'port' is the port number to listen on");
    exit (1);
}

void *get_in_addr (struct sockaddr *sa) {
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in*)sa)->sin_addr);

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main (int argc, char *argv[]) {

    char *endptr;
    int sockfd;
    long new_fd;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    pthread_t tid;
    const int yes = 1;
    int rc;

    if (argc != 3)
        usage ();

    endptr = NULL;
    const int max_threads = strtol (argv[1], &endptr, 10);
    if (max_threads < 1 || (endptr && *endptr != '\0')) {
        puts ("error: 'nclients' must be an integer greater than 0");
        usage ();
    }

    endptr = NULL;
    if (strtol (argv[2], &endptr, 10) < 1 || (endptr && *endptr != '\0')) {
        puts ("error: 'port' must be an integer greater than 0");
        usage ();
    }

    // initialize shared variable
    num_threads = 0;
    pthread_mutex_init (&num_threads_lock, NULL);

    memset (&hints, 0, sizeof (hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE;

    if ((rc = getaddrinfo (NULL, argv[2], &hints, &servinfo))) {
        fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (rc));
        return 1;
    }

    // create a socket to listen for incoming connections
    for (p = servinfo; p; p = p->ai_next) {
        if ((sockfd = socket (p->ai_family, p->ai_socktype,
                        p->ai_protocol)) == -1) {
            perror ("server: socket");
            continue;
        }

        if (setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                    sizeof (int)) == -1) {
            perror ("setsockopt");
            exit (1);
        }

        if (bind (sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close (sockfd);
            perror ("server: bind");
            continue;
        }

        break;
    }

    if (!p) {
        fprintf (stderr, "server: failed to bind\n");
        return 2;
    }

    freeaddrinfo (servinfo);

    if (listen (sockfd, BACKLOG) == -1) {
        perror ("listen");
        exit (1);
    }

    ctable_init ();

    for (;;) {

        // wait for a connection
        sin_size = sizeof (their_addr);
        new_fd = accept (sockfd, (struct sockaddr*) &their_addr, &sin_size);
        if (new_fd == -1) {
            perror ("accept");
            continue;
        }

        // close connection if thread limit reached
        pthread_mutex_lock (&num_threads_lock);
        if (num_threads >= max_threads) {
            fprintf (stderr, "thread limit reached; refusing connection\n");
            pthread_mutex_unlock (&num_threads_lock);
            close (new_fd);
            continue;
        }

        num_threads++;
        pthread_mutex_unlock (&num_threads_lock);

        struct timeval tv = { .tv_sec = 30, .tv_usec = 0 };
        setsockopt (new_fd, SOL_SOCKET, SO_RCVTIMEO, (char*) &tv, sizeof (tv));

        struct conn_info *targ = malloc (sizeof (struct conn_info));
        targ->sock = new_fd;
        inet_ntop (their_addr.ss_family,
                get_in_addr ((struct sockaddr*) &their_addr),
                targ->addr, sizeof targ->addr);
#ifdef P2PSERV_LOG
        printf ("C %s\n", targ->addr);
#endif
        // create a new thread to service the connection
        if (pthread_create (&tid, NULL, handle_request, targ))
            perror ("pthread_create");
    }

    return 0;
}
