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
#include <time.h>

#include "common.h"
#include "tcp.h"
#include "udp.h"
#include "response.h"
#include "client.h"

#define REQ_DELIM " \t\r\n"

/*-----------------------------------------------------------------------------
 * Send the standard OK ACK */
//-----------------------------------------------------------------------------
static void send_okay (int sock)
{
    struct response_node head;

    response_ok (&head);
    send_response (sock, head.next);
    free_response (head.next);
}

/*-----------------------------------------------------------------------------
 * Send the standard error ACK */
//-----------------------------------------------------------------------------
static void send_error (int sock)
{
    struct response_node head;

    response_bad (&head);
    send_response (sock, head.next);
    free_response (head.next);
}

/*-----------------------------------------------------------------------------
 * Process a 'CONNECT [port]' command */
//-----------------------------------------------------------------------------
static void process_connect (struct conn_info *info, char *args)
{
    char *port, *p;

    port = strtok_r (args, REQ_DELIM, &p);
    if (!port || add_client (&info->addr, port)) {
        send_error (info->sock);
        return;
    }
    send_okay (info->sock);
#ifdef P2PSERV_LOG
    printf (ANSI_GREEN "+ %s %s\n" ANSI_RESET, info->paddr, port);
#endif
}

/*-----------------------------------------------------------------------------
 * Process a 'DISCONNECT [port]' command */
//-----------------------------------------------------------------------------
static void process_disconnect (struct conn_info *info, char *args)
{
    char *port, *p;

    port = strtok_r (args, REQ_DELIM, &p);
    if (!port || remove_client (&info->addr, port)) {
        send_error (info->sock);
        return;
    }
    send_okay (info->sock);
#ifdef P2PSERV_LOG
    printf (ANSI_RED "- %s %s\n" ANSI_RESET, info->paddr, port);
#endif
}

/*-----------------------------------------------------------------------------
 * Process a 'LIST [n]' command */
//-----------------------------------------------------------------------------
static void process_list (struct conn_info *info, char *args)
{
    struct response_node head;
    struct response_node *jlist;
    char *n, *p;
    
    n = strtok_r (args, REQ_DELIM, &p);
    if (!n || clients_to_json (&jlist, NULL, n)) {
        send_error (info->sock);
        return;
    }

    make_response_with_body (&head, jlist);
#ifdef P2PSERV_LOG
    printf (ANSI_YELLOW "L %s\n" ANSI_RESET, info->paddr);
#endif
    send_response (info->sock, head.next);
    free_response (head.next);
}

/*-----------------------------------------------------------------------------
 * Process a 'DISCOVER [port] [n]' command */
//-----------------------------------------------------------------------------
static void process_discover (struct conn_info *info, char *args)
{
    struct response_node response_head;
    struct response_node *jlist;
    char *port, *n, *p;
    char *endptr;
    long lport;
    
    port = strtok_r (args, REQ_DELIM, &p);
    n    = strtok_r (NULL, REQ_DELIM, &p);
    if (!n || !port)
        goto bail_error;

    lport = strtol (port, &endptr, 10);
    if (lport < PORT_MIN || lport > PORT_MAX || (endptr && *endptr != '\0'))
        goto bail_error;

    set_in_port ((struct sockaddr*)&info->addr, htons ((in_port_t) lport));
    if (clients_to_json (&jlist, &info->addr, n))
        goto bail_error;

    make_response_with_body (&response_head, jlist);
#ifdef P2PSERV_LOG
    printf (ANSI_YELLOW "L %s %s\n" ANSI_RESET, info->paddr, port);
#endif
    send_response (info->sock, response_head.next);
    free_response (response_head.next);
    return;

bail_error:
    send_error (info->sock);
}

/*-----------------------------------------------------------------------------
 * Handles a TCP connection (callback for tcp_server_main()) */
//-----------------------------------------------------------------------------
static void *handle_connection (void *data)
{
    struct conn_info *info = data;

    char msg_buf[TCP_MSG_MAX];
    char *cmd, *args, *p;
    int rv;

    for(;;) {

        // read message from socket
        if (!(rv = tcp_read_message (info->sock, msg_buf)))
            break;

        // parse message
        cmd  = strtok_r (msg_buf, REQ_DELIM, &p);
        args = strtok_r (NULL, "", &p);

        // dispatch
        if (!cmd)
            send_error (info->sock);
        else if (cmd_equal (cmd, "CONNECT", 7))
            process_connect (info, args);
        else if (cmd_equal (cmd, "DISCONNECT", 10))
            process_disconnect (info, args);
        else if (cmd_equal (cmd, "LIST", 4))
            process_list (info, args);
        else if (cmd_equal (cmd, "DISCOVER", 8))
            process_discover (info, args);
        else if (cmd_equal (cmd, "EXIT", 4))
            break;
        else
            send_error (info->sock);
    }

    // clean up
    close (info->sock);
    pthread_mutex_lock (&tcp_threads_lock);
    tcp_threads--;
    pthread_mutex_unlock (&tcp_threads_lock);
#ifdef P2PSERV_LOG
    printf ("D %s\n", info->paddr); fflush (stdout);
#endif
    free (info);
    pthread_exit (NULL);
}

static void *handle_message (void *data)
{
    struct msg_info *mi = data;
    char *cmd, *port, *p;

    // parse message
    cmd  = strtok_r (mi->msg, REQ_DELIM, &p);
    port = strtok_r (NULL, REQ_DELIM, &p);

    if (!cmd || !cmd_equal (cmd, "CONNECT", 7))
        goto cleanup;

    if (!port || add_client (&mi->addr, port))
        goto cleanup;

#ifdef P2PSERV_LOG
    printf (ANSI_GREEN "+ %s %s\n" ANSI_RESET, mi->paddr, port);
#endif

cleanup:
    pthread_mutex_lock (&udp_threads_lock);
    udp_threads--;
    pthread_mutex_unlock (&udp_threads_lock);
    free (mi);
#ifdef P2PSERV_LOG
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
    int sockfd = udp_server_init (data);
    udp_server_main (sockfd, 10000, handle_message);
}

/*-----------------------------------------------------------------------------
 * Main... */
//-----------------------------------------------------------------------------
int main (int argc, char *argv[])
{
    char *endptr;
    int sockfd;
    int max_threads;
    pthread_t tid;

    if (argc != 3)
        usage ();

    endptr = NULL;
    max_threads = strtol (argv[1], &endptr, 10);
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

    clients_init ();

    if (pthread_create (&tid, NULL, udp_serve, argv[2]))
        perror ("pthread_create");
    else if (pthread_detach (tid))
        perror ("pthread_detach");

    sockfd = tcp_server_init (argv[2]);
    tcp_server_main (sockfd, max_threads, handle_connection);
}
