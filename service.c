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
#include "client.h"

#define BUF_SIZE 100
#define MSG_MAX  100

/* receive buffer (used only by recv_char) */
struct recv_buf {
    char data[BUF_SIZE];
    ssize_t pos;
    ssize_t len;
};

/* the message delimiter sequence */
static const struct {
    const char *str;
    size_t len;
} delim = { "\r\n\r\n", 4 };

static inline bool cmd_equal (const char *str, const char *cmd, size_t len) {
    return !strncmp (str, cmd, len) && str[len] == '\0';
}

/*-----------------------------------------------------------------------------
 * Reads a byte from the buffer, receiving more bytes from the socket if the
 * buffer is empty */
//-----------------------------------------------------------------------------
static signed char recv_char (int sock, struct recv_buf *buf) {

    if (buf->pos == buf->len) {
        buf->pos = 0;
        if ((buf->len = recv (sock, buf->data, BUF_SIZE, 0)) == -1)
            return -1;
        if (!buf->len)
            return -2;
    }
    buf->pos++;
    return buf->data[buf->pos - 1];
}

/*-----------------------------------------------------------------------------
 * Reads into the buffer (buf) until either the delimiter sequence is found or
 * the maximum message size (MSG_MAX) is reached.  Returns the message size on
 * a successful read, or 0 if the client closed the connection */
//-----------------------------------------------------------------------------
static size_t read_msg (int sock, struct recv_buf *recv_buf, char *msg_buf) {
    size_t i, delim_pos;

    for (i = 0, delim_pos = 0; i < MSG_MAX-1 && delim_pos < delim.len; i++) {

        signed char c = recv_char (sock, recv_buf);
        if (c < 0)
            return 0;

        if (c == delim.str[delim_pos])
            delim_pos++;
        else
            delim_pos = 0;

        msg_buf[i] = c;
    }

    msg_buf[i] = '\0';
    return i;
}

/*-----------------------------------------------------------------------------
 * Sends `len' bytes from `buf' into the given socket */
//-----------------------------------------------------------------------------
static int send_msg (int sock, char *buf, size_t len) {
    ssize_t rc;
    size_t sent = 0;

    while (sent != len) {
        if ((rc = send (sock, buf + sent, len - sent, 0)) == -1)
            return -1;
        sent += rc;
    }
    return 0;
}

/*-----------------------------------------------------------------------------
 * Sends a linked list of response strings into the given socket */
//-----------------------------------------------------------------------------
static int send_response (int sock, struct response_node *node) {
    while (node) {
        if (send_msg (sock, node->data, node->size) == -1)
            return -1;
        node = node->next;
    }
    return 0;
}

/*-----------------------------------------------------------------------------
 * Constructs a link in the response list from the arguments */
//-----------------------------------------------------------------------------
static void make_simple_response (struct response_node *prev, const char *data,
        size_t data_size) {
    prev->next = malloc (sizeof (struct response_node));
    prev->next->data = strdup (data);
    prev->next->size = data_size;
    prev->next->next = NULL;
}

/*-----------------------------------------------------------------------------
 * Constructs a response given a sentinel "head" node and a "rest" node
 * containing the actual message, inserting a header in between */
//-----------------------------------------------------------------------------
static void make_response (struct response_node *head,
        struct response_node *rest) {
    struct response_node *hdr, *it;
    size_t rest_len;

    // count size of "rest"
    for (rest_len = 0, it = rest; it; it = it->next)
        rest_len += it->size;

    // fill out header node
    hdr = malloc (sizeof (struct response_node));
    hdr->data = malloc (100);
    hdr->size = snprintf (hdr->data, 100,
            "{\"status\":\"okay\",\"size\":%lu}\r\n\r\n", rest_len);
    hdr->next = rest;

    head->next = hdr;
}

/*-----------------------------------------------------------------------------
 * Frees the memory associated with a response list */
//-----------------------------------------------------------------------------
static void free_response (struct response_node *node) {
    struct response_node *next;

    while (node) {
        next = node->next;
        free (node->data);
        free (node);
        node = next;
    }
}

static inline void response_bad (struct response_node *prev) {
    make_simple_response (prev, "{\"status\":\"error\"}\r\n\r\n", 22);
}

static inline void response_ok (struct response_node *prev) {
    make_simple_response (prev, "{\"status\":\"okay\"}\r\n\r\n", 21);
}

/*-----------------------------------------------------------------------------
 * Handles requests from the client */
//-----------------------------------------------------------------------------
void *handle_request (void *data) {

    bool done;
    struct conn_info *info;

    char msg_buf[MSG_MAX];
    char *cmd, *port, *p;

    struct response_node response_head;
    struct recv_buf recv_buf = { .pos = 0, .len = 0 };

    info = data;
    done = false;

    while (!done) {

        // go to cleanup if connection was closed
        if (!read_msg (info->sock, &recv_buf, msg_buf))
            break;

        // parse message
        cmd  = strtok_r (msg_buf, " \r\n", &p);
        port = strtok_r (NULL,    " \r\n", &p);

        // construct response
        response_head.next = NULL;
        if (!cmd) {
            response_bad (&response_head);
        } else if (cmd_equal (cmd, "CONNECT", 7)) {

            if (!port || add_client (info->addr, port)) {
                response_bad (&response_head);
            } else {
                response_ok (&response_head);
#ifdef P2PSERV_LOG
                printf (ANSI_GREEN "+ %s %s\n" ANSI_RESET, info->addr, port);
#endif
            }
        } else if (cmd_equal (cmd, "DISCONNECT", 10)) {

            if (!port || remove_client (info->addr, port)) {
                response_bad (&response_head);
            } else {
                response_ok (&response_head);
#ifdef P2PSERV_LOG
                printf (ANSI_RED "- %s %s\n" ANSI_RESET, info->addr, port);
#endif
            }
        } else if (cmd_equal (cmd, "LIST", 4)) {
            struct response_node *jlist;
            if (!port || clients_to_json (&jlist, info->addr, port)) {
                response_bad (&response_head);
            } else {
                make_response (&response_head, jlist);
#ifdef P2PSERV_LOG
                printf (ANSI_YELLOW "L %s %s\n" ANSI_RESET, info->addr, port);
#endif
            }
        } else if (cmd_equal (cmd, "EXIT", 4)) {
            response_ok (&response_head);
            done = true;
        } else {
            response_bad (&response_head);
        }

        // send response
        send_response (info->sock, response_head.next);
        free_response (response_head.next);
    }

#ifdef P2PSERV_LOG
    printf ("D %s\n", info->addr); fflush (stdout);
#endif
    close (info->sock);
    free (info);
    pthread_mutex_lock (&num_threads_lock);
    num_threads--;
    pthread_mutex_unlock (&num_threads_lock);
    pthread_exit (NULL);
}
