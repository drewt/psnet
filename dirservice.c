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
#include "response.h"
#include "client.h"
#include "ctable.h"

/*-----------------------------------------------------------------------------
 * Handles requests from the client */
//-----------------------------------------------------------------------------
static void *handle_connection (void *data)
{
    struct conn_info *info;

    char msg_buf[TCP_MSG_MAX];
    char *cmd, *port, *p;
    int rv;

    struct response_node response_head;

    info = data;

    for(;;) {

        // go to cleanup if connection was closed
        if (!(rv = tcp_read_message (info->sock, msg_buf)))
            break;

        // parse message
        cmd  = strtok_r (msg_buf, " \r\n", &p);
        port = strtok_r (NULL,    " \r\n", &p);

        // construct response
        response_head.next = NULL;
        if (!cmd) {
            response_bad (&response_head);
        } else if (cmd_equal (cmd, "CONNECT", 7)) {

            if (!port || add_client (&info->addr, port)) {
                response_bad (&response_head);
            } else {
                response_ok (&response_head);
#ifdef P2PSERV_LOG
                printf (ANSI_GREEN "+ %s %s\n" ANSI_RESET, info->paddr, port);
#endif
            }
        } else if (cmd_equal (cmd, "DISCONNECT", 10)) {

            if (!port || remove_client (&info->addr, port)) {
                response_bad (&response_head);
            } else {
                response_ok (&response_head);
#ifdef P2PSERV_LOG
                printf (ANSI_RED "- %s %s\n" ANSI_RESET, info->paddr, port);
#endif
            }
        } else if (cmd_equal (cmd, "LIST", 4)) {
            struct response_node *jlist;
            char *n = strtok_r (NULL, " \r\n", &p);
            if (!port || !n || clients_to_json (&jlist, n)) {
                response_bad (&response_head);
            } else {
                make_response_with_body (&response_head, jlist);
#ifdef P2PSERV_LOG
                printf (ANSI_YELLOW "L %s %s\n" ANSI_RESET, info->paddr, port);
#endif
            }
        } else if (cmd_equal (cmd, "EXIT", 4)) {
            break;
        } else {
            response_bad (&response_head);
        }

        // send response
        if (send_response (info->sock, response_head.next) == -1) {
            free_response (response_head.next);
            break;
        }
        free_response (response_head.next);
    }

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

static void __attribute((noreturn)) usage (void)
{
    puts ("usage: infradir [nclients] [port]\n"
          "\twhere 'nclients' is the maximum number of clients\n"
          "\tand 'port' is the port number to listen on");
    exit (EXIT_FAILURE);
}

int main (int argc, char *argv[])
{
    char *endptr;
    int sockfd;
    int max_threads;

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

    ctable_init ();
    sockfd = tcp_server_init (argv[2]);

    tcp_server_main (sockfd, max_threads, handle_connection);
}
