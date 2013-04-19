#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/socket.h>

#include "response.h"

/*-----------------------------------------------------------------------------
 * Sends `len' bytes from `buf' into the given socket */
//-----------------------------------------------------------------------------
int tcp_send_bytes (int sock, char *buf, size_t len)
{
    ssize_t rc;
    size_t sent = 0;

    while (sent != len) {
        if ((rc = send (sock, buf + sent, len - sent, MSG_NOSIGNAL)) == -1) {
            perror ("send");
            return -1;
        }
        sent += rc;
    }
    return 0;
}

/*-----------------------------------------------------------------------------
 * Sends a linked list of response strings into the given socket */
//-----------------------------------------------------------------------------
int send_response (int sock, struct response_node *node)
{
    while (node) {
        if (tcp_send_bytes (sock, node->data, node->size) == -1) {
            fprintf (stderr, "send_msg: failed to send\n");
            return -1;
        }
        node = node->next;
    }
    return 0;
}

/*-----------------------------------------------------------------------------
 * Constructs a response given a sentinel "head" node and a "rest" node
 * containing the actual message, inserting a header in between */
//-----------------------------------------------------------------------------
void make_response_with_body (struct response_node *head,
        struct response_node *body)
{
    struct response_node *hdr, *it;
    size_t rest_len;

    // count size of "rest"
    for (rest_len = 0, it = body; it; it = it->next)
        rest_len += it->size;

    // fill out header node
    hdr = malloc (sizeof (struct response_node));
    hdr->data = malloc (100);
#ifdef LISP_OUTPUT
    hdr->size = snprintf (hdr->data, 100,
            "(:status \"okay\" :size %lu)\r\n\r\n", rest_len);
#else
    hdr->size = snprintf (hdr->data, 100,
            "{\"status\":\"okay\",\"size\":%lu}\r\n\r\n", rest_len);
#endif
    hdr->next = body;

    head->next = hdr;
}

void send_error (int sock, int no, const char *str)
{
#ifdef LISP_OUTPUT
#define ERR_FMT "(:status \"error\" :code %d :reason \"%s\")"
#define ERR_LEN 100
#else
#define ERR_FMT "{\"status\":\"error\",\"code\":%d,\"reason\":\"%s\"}"
#define ERR_LEN 100
#endif
    int rv;
    char s[ERR_LEN];
    rv = sprintf (s, ERR_FMT, no, str);
    tcp_send_bytes (sock, s, rv);
#undef ERR_FMT
#undef ERR_LEN
}

void send_ok (int sock)
{
#ifdef LISP_OUTPUT
    tcp_send_bytes (sock, "(:status \"okay\")\r\n\r\n", 20);
#else
    tcp_send_bytes (sock, "{\"status\":\"okay\"}\r\n\r\n", 21);
#endif
}

/*-----------------------------------------------------------------------------
 * Frees the memory associated with a response list */
//-----------------------------------------------------------------------------
void free_response (struct response_node *node)
{
    struct response_node *next;

    while (node) {
        next = node->next;
        free (node->data);
        free (node);
        node = next;
    }
}
