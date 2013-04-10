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
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <pthread.h>

#include "common.h"
#include "tcp.h"

#define BACKLOG 10

#define LOG_FILE_PATH "/tmp/p2pservlog"

int tcp_threads;
pthread_mutex_t tcp_threads_lock;

#ifdef DAEMON
/*-----------------------------------------------------------------------------
 * Run the server in the background */
//-----------------------------------------------------------------------------
void daemonize (void)
{
    pid_t pid, sid;

    pid = fork ();
    if (pid == -1) {
        perror ("fork");
        exit (EXIT_FAILURE);
    }
    if (pid > 0)
        exit (EXIT_SUCCESS);

    umask (0);

    freopen (LOG_FILE_PATH, "w", stdout);
    freopen (LOG_FILE_PATH, "w", stderr);
    fclose (stdin);

    sid = setsid ();
    if (sid == -1) {
        perror ("setsid");
        exit (EXIT_FAILURE);
    }

    if (chdir ("/") == -1) {
        perror ("chdir");
        exit (EXIT_FAILURE);
    }
}
#endif

/*-----------------------------------------------------------------------------
 * Initialize the server to listen on the given port */
//-----------------------------------------------------------------------------
int tcp_server_init (char *port)
{
    struct addrinfo hints, *servinfo, *p;
    const int yes = 1;
    int sockfd;
    int rc;

    memset (&hints, 0, sizeof (hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE;

    if ((rc = getaddrinfo (NULL, port, &hints, &servinfo))) {
        fprintf (stderr, "tcpserver: getaddrinfo: %s\n", gai_strerror (rc));
        exit (EXIT_FAILURE);
    }

    // create a socket to listen for incoming connections
    for (p = servinfo; p; p = p->ai_next) {
        if ((sockfd = socket (p->ai_family, p->ai_socktype,
                        p->ai_protocol)) == -1) {
            perror ("tcpserver: socket");
            continue;
        }

        if (setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                    sizeof (int)) == -1) {
            perror ("tcpserver: setsockopt");
            exit (EXIT_FAILURE);
        }

        if (bind (sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close (sockfd);
            perror ("tcpserver: bind");
            continue;
        }

        break;
    }

    if (!p) {
        fprintf (stderr, "tcpserver: failed to bind\n");
        exit (EXIT_FAILURE);
    }

    freeaddrinfo (servinfo);

    if (listen (sockfd, BACKLOG) == -1) {
        perror ("tcpserver: listen");
        exit (EXIT_FAILURE);
    }

    tcp_threads = 0;
    pthread_mutex_init (&tcp_threads_lock, NULL);

    return sockfd;
}

/*-----------------------------------------------------------------------------
 * The server's main accept() loop */
//-----------------------------------------------------------------------------
void __attribute((noreturn)) tcp_server_main (int sockfd, int max_threads,
        void*(*cb)(void*))
{
    socklen_t sin_size;
    struct conn_info *targ;
    pthread_t tid;

    struct timeval tv = { .tv_sec = 30, .tv_usec = 0 };

    for (;;) {

        targ = malloc (sizeof (struct conn_info));

        // wait for a connection
        sin_size = sizeof (targ->addr);
        targ->sock = accept (sockfd, (struct sockaddr*) &targ->addr, &sin_size);
        if (targ->sock == -1) {
            perror ("accept");
            free (targ);
            continue;
        }

        // close connection if thread limit reached
        pthread_mutex_lock (&tcp_threads_lock);
        if (tcp_threads >= max_threads) {
            fprintf (stderr, "thread limit reached; refusing connection\n");
            pthread_mutex_unlock (&tcp_threads_lock);
            close (targ->sock);
            free (targ);
            continue;
        }

        tcp_threads++;
        pthread_mutex_unlock (&tcp_threads_lock);

        setsockopt (targ->sock, SOL_SOCKET, SO_RCVTIMEO, (char*) &tv,
                sizeof (tv));

#ifdef P2PSERV_LOG
        inet_ntop (targ->addr.ss_family,
                get_in_addr ((struct sockaddr*) &targ->addr),
                targ->paddr, sizeof targ->paddr);
        printf ("C %s\n", targ->paddr);
#endif
        // create a new thread to service the connection
        if (pthread_create (&tid, NULL, cb, targ))
            perror ("pthread_create");
        if (pthread_detach (tid))
            perror ("pthread_detach");
    }
}