#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#include "response.h"

/*-----------------------------------------------------------------------------
 * Sends `len' bytes from `buf' into the given socket */
//-----------------------------------------------------------------------------
static int tcp_send_bytes (int sock, char *buf, size_t len)
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
 * Constructs a link in the response list from the arguments */
//-----------------------------------------------------------------------------
void make_simple_response (struct response_node *prev, const char *data,
        size_t data_size)
{
    prev->next = malloc (sizeof (struct response_node));
    prev->next->data = strdup (data);
    prev->next->size = data_size;
    prev->next->next = NULL;
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
    struct response_node head;
    rv = sprintf (s, ERR_FMT, no, str);
    make_simple_response (&head, s, rv);
    send_response (sock, head.next);
    free_response (head.next);
#undef ERR_FMT
#undef ERR_LEN
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
