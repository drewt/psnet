/* service.c : handles connections from clients
 *
 * Author: Drew Thoreson
 */

#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>

#include "common.h"
#include "tcp.h"
#include "udp.h"
#include "response.h"
#include "client.h"

#define REQ_DELIM " \t\r\n"

#define dir_error(sock, no) send_error (sock, no, psdir_strerror[no])
enum input_errors { ENOCMD, ENONUM, ENOPORT, EBADCMD, EBADNUM, EBADPORT };
static const char *psdir_strerror[] = {
    [ENOCMD]   = "no command given",
    [ENONUM]   = "missing argument 'n'",
    [ENOPORT]  = "missing argument 'port'",
    [EBADCMD]  = "invalid command",
    [EBADNUM]  = "invalid argument 'n'",
    [EBADPORT] = "invalid argument 'port'" // 23
};

int max_threads;
int num_threads;
pthread_mutex_t num_threads_lock;

/*-----------------------------------------------------------------------------
 * Process a 'CONNECT [port]' command */
//-----------------------------------------------------------------------------
static void process_connect (struct msg_info *mi, char *port)
{
    if (add_client (&mi->addr, port))
        return;

#ifdef PSNETLOG
    printf (ANSI_GREEN "+ %s %s\n" ANSI_RESET, mi->paddr, port);
#endif
}

/*-----------------------------------------------------------------------------
 * Process a 'DISCONNECT [port]' command */
//-----------------------------------------------------------------------------
static void process_disconnect (struct msg_info *mi, char *port)
{
    if (remove_client (&mi->addr, port))
        return;

#ifdef PSNETLOG
    printf (ANSI_RED "- %s %s\n" ANSI_RESET, mi->paddr, port);
#endif
}

/*-----------------------------------------------------------------------------
 * Process a 'LIST [n]' command */
//-----------------------------------------------------------------------------
static void process_list (struct msg_info *mi, char *args)
{
    struct response_node head;
    struct response_node *jlist;
    char *n, *p;
    
    n = strtok_r (args, REQ_DELIM, &p);
    if (n == NULL) {
        dir_error (mi->sock, ENONUM);
        return;
    }
    if (clients_to_json (&jlist, NULL, n)) {
        dir_error (mi->sock, EBADNUM);
        return;
    }

    make_response_with_body (&head, jlist);
#ifdef PSNETLOG
    printf (ANSI_YELLOW "L %s\n" ANSI_RESET, mi->paddr);
#endif
    send_response (mi->sock, head.next);
    free_response (head.next);
}

/*-----------------------------------------------------------------------------
 * Process a 'DISCOVER [port] [n]' command */
//-----------------------------------------------------------------------------
static void process_discover (struct msg_info *mi, char *args)
{
    struct response_node head;
    struct response_node *jlist;
    char *port, *n, *p;
    char *endptr;
    long lport;
    int eno;
    
    port = strtok_r (args, REQ_DELIM, &p);
    n    = strtok_r (NULL, REQ_DELIM, &p);
    if (port == NULL) {
        eno = ENOPORT;
        goto bail_error;
    }
    if (n == NULL) {
        eno = ENONUM;
        goto bail_error;
    }

    lport = strtol (port, &endptr, 10);
    if (lport < PORT_MIN || lport > PORT_MAX || (endptr && *endptr != '\0')) {
        eno = EBADPORT;
        goto bail_error;
    }

    set_in_port ((struct sockaddr*)&mi->addr, htons ((in_port_t) lport));
    if (clients_to_json (&jlist, &mi->addr, n)) {
        eno = EBADNUM;
        goto bail_error;
    }

    make_response_with_body (&head, jlist);
#ifdef PSNETLOG
    printf (ANSI_YELLOW "L %s %s\n" ANSI_RESET, mi->paddr, port);
#endif
    send_response (mi->sock, head.next);
    free_response (head.next);
    return;

bail_error:
    dir_error (mi->sock, eno);
}

/*-----------------------------------------------------------------------------
 * Handles a TCP connection (callback for tcp_server_main()) */
//-----------------------------------------------------------------------------
static void *handle_connection (void *data)
{
    struct msg_info *mi = data;
    char *cmd, *args, *p;
    int rv;

    for(;;) {

        // read message from socket
        if (!(rv = tcp_read_message (mi->sock, mi->msg)))
            break;

        // parse message
        cmd  = strtok_r (mi->msg, REQ_DELIM, &p);
        args = strtok_r (NULL, "", &p);

        // dispatch
        if (!cmd) {
            dir_error (mi->sock, ENOCMD);
            break;
        } else if (cmd_equal (cmd, "LIST", 4)) {
            process_list (mi, args);
        } else if (cmd_equal (cmd, "DISCOVER", 8)) {
            process_discover (mi, args);
        } else if (cmd_equal (cmd, "EXIT", 4)) {
            break;
        } else {
            dir_error (mi->sock, EBADCMD);
            break;
        }
    }

    // clean up
    close (mi->sock);
    pthread_mutex_lock (&num_threads_lock);
    num_threads--;
    pthread_mutex_unlock (&num_threads_lock);
#ifdef PSNETLOG
    printf ("D %s\n", mi->paddr);
#endif
    free (mi);
    pthread_exit (NULL);
}

/*-----------------------------------------------------------------------------
 * Handles a UDP message (callback for udp_server_main()) */
//-----------------------------------------------------------------------------
static void *handle_message (void *data)
{
    struct msg_info *mi = data;
    char *cmd, *port, *p;

    // parse message
    cmd  = strtok_r (mi->msg, REQ_DELIM, &p);
    port = strtok_r (NULL, REQ_DELIM, &p);

    if (!cmd || !port)
        goto cleanup;
    else if (cmd_equal (cmd, "CONNECT", 7))
        process_connect (mi, port);
    else if (cmd_equal (cmd, "DISCONNECT", 10))
        process_disconnect (mi, port);

cleanup:
    pthread_mutex_lock (&num_threads_lock);
    num_threads--;
    pthread_mutex_unlock (&num_threads_lock);
    free (mi);
#ifdef PSNETLOG
    printf ("-M %s\n", mi->paddr);
#endif
    pthread_exit (NULL);
}

/*-----------------------------------------------------------------------------
 * Usage... */
//-----------------------------------------------------------------------------
static _Noreturn void usage (void)
{
    puts ("usage: infradir [nclients] [port]\n"
          "\twhere 'nclients' is the maximum number of clients\n"
          "\tand 'port' is the port number to listen on");
    exit (EXIT_FAILURE);
}

void *udp_serve (void *data)
{
    int sockfd;

    pthread_detach (pthread_self ());

    sockfd = udp_server_init (data);
    udp_server_main (sockfd, handle_message);
}

/*-----------------------------------------------------------------------------
 * Main... */
//-----------------------------------------------------------------------------
int main (int argc, char *argv[])
{
    char *endptr;
    int sockfd;
    pthread_t tid;

    if (argc != 3)
        usage ();

    endptr = NULL;
    max_threads = (int) strtol (argv[1], &endptr, 10);
    if (max_threads < 1 || (endptr && *endptr != '\0')) {
        puts ("error: 'nclients' must be an integer greater than 0");
        usage ();
    }

    endptr = NULL;
    if (strtol (argv[2], &endptr, 10) < 1 || (endptr && *endptr != '\0')) {
        puts ("error: 'port' must be an integer greater than 0");
        usage ();
    }

#ifdef DAEMON
    daemonize ();
#endif

    num_threads = 0;
    pthread_mutex_init (&num_threads_lock, NULL);

    clients_init ();

    if (pthread_create (&tid, NULL, udp_serve, argv[2]))
        perror ("pthread_create");

    sockfd = tcp_server_init (argv[2]);
    tcp_server_main (sockfd, handle_connection);
}
