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

static const char * const good = "OK\r\n";
static const char * const bad  = "ER\r\n";

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
 * Handles requests from the client */
//-----------------------------------------------------------------------------
void *handle_request (void *data) {

    struct {
        char   *str;
        size_t len;
    } response;

    bool done;
    struct conn_info *info;

    char msg_buf[MSG_MAX];
    char *cmd, *port;

    struct recv_buf recv_buf = { .pos = 0, .len = 0 };

    info = data;
    done = false;

    while (!done) {
        response.str = strdup (good);
        response.len = 5;
        if (!read_msg (info->sock, &recv_buf, msg_buf))
            break;

        if (!(cmd = strtok (msg_buf, " \r\n"))) {
            response.str = strdup (bad);
        } else if (cmd_equal (cmd, "CONNECT", 7)) {

            if (!(port = strtok (NULL, " \r\n"))) {
                response.str = strdup (bad);
            } else if (add_client (info->addr, port)) {
                response.str = strdup (bad);
            }
#ifdef P2PSERV_LOG
else { printf (ANSI_GREEN "+ %s %s\n" ANSI_RESET, info->addr, port); }
#endif
        } else if (cmd_equal (cmd, "DISCONNECT", 10)) {

            if (!(port = strtok (NULL, " \r\n"))) {
                response.str = strdup (bad);
            } else if (remove_client (info->addr, port)) {
                response.str = strdup (bad);
            }
#ifdef P2PSERV_LOG
else { printf (ANSI_RED "- %s %s\n" ANSI_RESET, info->addr, port);}
#endif
        } else if (cmd_equal (cmd, "LIST", 4)) {
            response.str = clients_to_json ();
            response.len = strlen (response.str) + 1;
        } else if (cmd_equal (cmd, "EXIT", 4)) {
            done = true;
        } else {
            response.str = strdup (bad);
        }

        send_msg (info->sock, response.str, response.len);
        free (response.str);
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
