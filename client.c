#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "common.h"
#include "client.h"
#include "ctable.h"

#define PORT_MIN 1024
#define PORT_MAX 65535

/* Client structure: a network node.
 *
 *   Client is an opaque type; it is not used whatsoever in the networking
 * code, and its definition is unavailable to the back-end code.  This module
 * provides the glue between the two.
 *
 */
struct client {
    in_addr_t ip;   // XXX: IPv4 only
    in_port_t port;
};

unsigned int ctable_hash (const struct client *client) {
    return client->ip + client->port;
}

bool ctable_equals (const struct client *a, const struct client *b) {
    return a->ip == b->ip && a->port == b->port;
}

void ctable_act (const struct client *client) {
#ifdef P2PSERV_LOG
    char addr[20];
    inet_ntop (AF_INET, &client->ip, addr, 20);
    printf (ANSI_RED "X %s %d\n" ANSI_RESET, addr, client->port);
    fflush (stdout);
#endif
}

void ctable_free (struct client *client) {
    free (client);
}

/*-----------------------------------------------------------------------------
 * Fills out a client structure with the given ip and port number, ensuring
 * that the arguments are valid */
//-----------------------------------------------------------------------------
static int make_client (struct client *client, const char *ip,
        const char *port) {
    struct in_addr addr;
    char *endptr;
    long lport;

    if (!inet_aton (ip, &addr))
        return CL_BADIP;

    lport = strtol (port, &endptr, 10);
    if (lport < PORT_MIN || lport > PORT_MAX || *endptr != '\0')
        return CL_BADPORT;

    *client = (struct client)
        { .ip = addr.s_addr, .port = (in_port_t) lport };

    return CL_OK;
}

/*-----------------------------------------------------------------------------
 * Adds a client to the network */
//-----------------------------------------------------------------------------
int add_client (const char *ip, const char *port) {
    struct client *client;
    int rc;

    client = malloc (sizeof (struct client));
    if ((rc = make_client (client, ip, port)))
        return rc;

    ctable_insert (client);

    return CL_OK;
}

/*-----------------------------------------------------------------------------
 * Removes a client from the network */
//-----------------------------------------------------------------------------
int remove_client (const char *ip, const char *port) {
    struct client client;
    int rc;

    if ((rc = make_client (&client, ip, port)))
        return rc;

    if (ctable_remove (&client))
        return CL_NOTFOUND;

    return CL_OK;
}

int print_client (const struct client *client, void *data) {
    FILE *stream = data;
    fprintf (stream, "{ %d, %d }\n", client->ip, client->port);
    return 0;
}

/* argument to the make_list() function */
struct make_list_arg {
    struct response_node *prev;
    int i;
    int n;
};

/*-----------------------------------------------------------------------------
 * Constructs a JSON representation of the given client structure and inserts
 * it into the list given in the argument */
//-----------------------------------------------------------------------------
static int make_list (const struct client *client, void *data) {

    struct make_list_arg *arg = data;
    if (arg->i > arg->n)
        return 1;
    arg->i++;

    char addr[20];
    inet_ntop (AF_INET, &client->ip, addr, 20);

    struct response_node *node = malloc (sizeof (struct response_node));

    node->data = malloc (38);
#ifdef LISP_OUTPUT
    node->size = snprintf (node->data, 100, "(:ip \"%s\" :port %d) ",
            addr, client->port);
#else
    node->size = snprintf (node->data, 100, "{\"ip\":\"%s\",\"port\":%d},",
            addr, client->port);
#endif
    node->next = NULL;

    arg->prev->next = node;
    arg->prev = node;

    return 0;
}

/*-----------------------------------------------------------------------------
 * Constructs a JSON array from the server's list of clients, excluding the
 * client given by the supplied IP address and port number */
//-----------------------------------------------------------------------------
int clients_to_json (struct response_node **dest, const char *n) {
    int num;
    char *endptr;
    struct make_list_arg arg;
    struct response_node *head;

    num = strtol (n, &endptr, 10);
    if (num < 0 || *endptr != '\0')
        return CL_BADNUM;

    arg.i = 0;
    arg.n = num;
    arg.prev = malloc (sizeof (struct response_node));
#ifdef LISP_OUTPUT
    arg.prev->data = strdup ("(");
#else
    arg.prev->data = strdup ("[");
#endif
    arg.prev->size = 1;
    arg.prev->next = NULL;
    head = arg.prev;

    ctable_foreach (make_list, &arg);

    if (arg.prev != head)
        arg.prev->size--; // ignore trailing separator
    arg.prev->next = malloc (sizeof (struct response_node));
#ifdef LISP_OUTPUT
    arg.prev->next->data = strdup (")\r\n");
#else
    arg.prev->next->data = strdup ("]\r\n");
#endif
    arg.prev->next->size = 3;
    arg.prev->next->next = NULL;

    *dest = head;
    return CL_OK;
}
