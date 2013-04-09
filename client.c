#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "common.h"
#include "client.h"
#include "response.h"
#include "ctable.h"

unsigned int ctable_hash (const struct sockaddr_storage *client)
{
    if (client->ss_family == AF_INET) {
        struct sockaddr_in *c4 = (struct sockaddr_in*) client;
        return c4->sin_addr.s_addr + c4->sin_port;
    } else if (client->ss_family == AF_INET6) {
        struct sockaddr_in6 *c6 = (struct sockaddr_in6*) client;
        return (in_addr_t) c6->sin6_addr.s6_addr[0] + c6->sin6_port;
    }
    return 0;
}

bool ctable_equals (const struct sockaddr_storage *a,
        const struct sockaddr_storage *b)
{
    return sockaddr_equals ((const struct sockaddr*) a,
            (const struct sockaddr*) b);
}

void ctable_act (const struct sockaddr_storage *client)
{
#ifdef P2PSERV_LOG
    char addr[INET6_ADDRSTRLEN];
    inet_ntop (client->ss_family, get_in_addr ((struct sockaddr*) client),
            addr, sizeof addr);
    printf (ANSI_RED "X %s %d\n" ANSI_RESET, addr,
            get_in_port ((struct sockaddr*) client));
    fflush (stdout);
#endif
}

void ctable_free (struct sockaddr_storage *client)
{
    free (client);
}

static int make_client (struct sockaddr_storage *addr, const char *port)
{
    char *endptr;
    long lport;

    lport = strtol (port, &endptr, 10);
    if (lport < PORT_MIN || lport > PORT_MAX || *endptr != '\0')
        return -1;

    if (addr->ss_family == AF_INET)
        ((struct sockaddr_in*)addr)->sin_port = (in_port_t) lport;
    else if (addr->ss_family == AF_INET6)
        ((struct sockaddr_in6*)addr)->sin6_port = (in_port_t) lport;
    else
        return -1;
    return 0;
}

/*-----------------------------------------------------------------------------
 * Adds a client to the network */
//-----------------------------------------------------------------------------
int add_client (struct sockaddr_storage *addr, const char *port)
{
    struct sockaddr_storage *client;

    client = malloc (sizeof (struct sockaddr_storage));
    *client = *addr;

    if (make_client (client, port))
        return -1;

    ctable_insert (client);
    return 0;
}

/*-----------------------------------------------------------------------------
 * Removes a client from the network */
//-----------------------------------------------------------------------------
int remove_client (struct sockaddr_storage *addr, const char *port)
{
    struct sockaddr_storage client = *addr;

    if (make_client (&client, port))
        return -1;

    if (ctable_remove (&client))
        return -1;
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
static int make_list (const struct sockaddr_storage *client, void *data)
{
    struct make_list_arg *arg = data;
    if (arg->i >= arg->n)
        return 1;
    arg->i++;

    char addr[INET6_ADDRSTRLEN];
    inet_ntop (client->ss_family, get_in_addr ((struct sockaddr*) client),
                addr, sizeof addr);

    struct response_node *node = malloc (sizeof (struct response_node));

    node->data = malloc (100);
#ifdef LISP_OUTPUT
    node->size = snprintf (node->data, 100, "(:ip \"%s\" :port %d :ipv %d) ",
            addr, get_in_port ((struct sockaddr*) client),
            client->ss_family == AF_INET ? 4 : 6);
#else
    node->size = snprintf (node->data, 100,
            "{\"ip\":\"%s\",\"port\":%d,\"ipv\":%d},",
            addr, get_in_port ((struct sockaddr*) client),
            client->ss_family == AF_INET ? 4 : 6);
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
int clients_to_json (struct response_node **dest, const char *n)
{
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
