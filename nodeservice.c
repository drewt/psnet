#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "common.h"
#include "request.h"
#include "response.h"
#include "client.h"

void *handle_request (void *data) {

    struct conn_info *info;

    char msg_buf[REQ_MAX];
    char *cmd, *port, *p;

    struct response_node response_head;

    info = data;

    for(;;) {

        // go to cleanup if connection was closed
        if (!read_message (info->sock, msg_buf))
            break;

        // parse message
        cmd  = strtok_r (msg_buf, " \r\n", &p);
        port = strtok_r (NULL,    " \r\n", &p);

        // construct response
        response_head.next = NULL;
        if (!cmd) {
            response_bad (&response_head);
        } else if (cmd_equal (cmd, "KEEP-ALIVE", 10)) {

            if (!port || add_client (info->addr, port)) {
                response_bad (&response_head);
            } else {
                response_ok (&response_head);
#ifdef P2PSERV_LOG
                printf (ANSI_GREEN "+ %s %s\n" ANSI_RESET, info->addr, port);
#endif
            }
        } else if (cmd_equal (cmd, "LEAVE", 5)) {

            if (!port || remove_client (info->addr, port)) {
                response_bad (&response_head);
            } else {
                response_ok (&response_head);
#ifdef P2PSERV_LOG
                printf (ANSI_RED "- %s %s\n" ANSI_RESET, info->addr, port);
#endif
            }
        } else if (cmd_equal (cmd, "DISCOVER", 8)) {
            response_ok (&response_head); // TODO
        } else if (cmd_equal (cmd, "SEARCH", 6)) {
            response_ok (&response_head); // TODO
        } else if (cmd_equal (cmd, "EXIT", 4)) {
            break;
        } else {
            response_bad (&response_head);
        }

        // send response
        send_response (info->sock, response_head.next);
        free_response (response_head.next);
    }

    close (info->sock);
    pthread_mutex_lock (&num_threads_lock);
    num_threads--;
    pthread_mutex_unlock (&num_threads_lock);
#ifdef P2PSERV_LOG
    printf ("D %s\n", info->addr); fflush (stdout);
#endif
    free (info);
    pthread_exit(NULL);
}
