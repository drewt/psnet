/* This file is part of psnet
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * psnet is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * psnet.  If not, see <http://www.gnu.org/licenses/>
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "common.h"
#include "client.h"
#include "udp.h"
#include "response.h"
#include "deltalist.h"

static unsigned long delta_hash (const void *client);
static int delta_equals (const void *a, const void *b);
static void delta_act (const void *client);

static struct delta_list client_table = {
    .resolution = 1,
    .interval = 10,
    .size = 0,
    .delta = 0,
    .delta_head = NULL,
    .delta_tail = NULL,
    .hash = delta_hash,
    .equals = delta_equals,
    .act = delta_act,
    .free = free
};

static unsigned long delta_hash (const void *data)
{
    const struct sockaddr_storage *client = data;
    if (client->ss_family == AF_INET) {
        struct sockaddr_in *c4 = (struct sockaddr_in*) client;
        return c4->sin_addr.s_addr + c4->sin_port;
    } else if (client->ss_family == AF_INET6) {
        struct sockaddr_in6 *c6 = (struct sockaddr_in6*) client;
        return (in_addr_t) c6->sin6_addr.s6_addr[0] + c6->sin6_port;
    }
    return 0;
}

static int delta_equals (const void *a, const void *b)
{
    return sockaddr_equals (a, b);
}

static void delta_act (const void *data)
{
#ifdef PSNETLOG
    const struct sockaddr_storage *client = data;
    char addr[INET6_ADDRSTRLEN];
    inet_ntop (client->ss_family, get_in_addr ((struct sockaddr*) client),
            addr, sizeof addr);
    printf (ANSI_RED "X %s %d\n" ANSI_RESET, addr,
            ntohs (get_in_port ((struct sockaddr*) client)));
    fflush (stdout);
#endif
}

void clients_init (void)
{
    delta_init (&client_table);
}

static int make_client (struct sockaddr_storage *addr, const char *port)
{
    char *endptr;
    long lport;

    lport = strtol (port, &endptr, 10);
    if (lport < PORT_MIN || lport > PORT_MAX || *endptr != '\0')
        return -1;

    set_in_port ((struct sockaddr*) addr, htons ((in_port_t) lport));
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

    delta_update (&client_table, client);
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

    if (delta_remove (&client_table, &client))
        return -1;
    return 0;
}

/* argument to the make_list() function */
struct make_list_arg {
    struct response_node *prev;
    struct sockaddr_storage *ignore;
    int i;
    int n;
};

/*-----------------------------------------------------------------------------
 * Constructs a JSON representation of the given client structure and inserts
 * it into the list given in the argument */
//-----------------------------------------------------------------------------
static int make_list (const void *data, void *arg)
{
#ifdef LISP_OUTPUT
#define ELM_FMT "(:ip \"%s\" :port %d :ipv %d) "
#define ELM_STRLEN 22 + INET6_ADDRSTRLEN + PORT_STRLEN + 1
#else
#define ELM_FMT "{\"ip\":\"%s\",\"port\":%d,\"ipv\":%d},"
#define ELM_STRLEN 25 + INET6_ADDRSTRLEN + PORT_STRLEN + 1
#endif
    const struct sockaddr_storage *client = data;
    struct make_list_arg *ml_arg = arg;
    if (ml_arg->i >= ml_arg->n)
        return 1;
    if (ml_arg->ignore && delta_equals (client, ml_arg->ignore))
        return 0;
    ml_arg->i++;

    char addr[INET6_ADDRSTRLEN];
    inet_ntop (client->ss_family, get_in_addr ((struct sockaddr*) client),
                addr, sizeof addr);

    struct response_node *node = malloc (sizeof (struct response_node));

    node->data = malloc (ELM_STRLEN);
    node->size = sprintf (node->data, ELM_FMT, addr,
            ntohs (get_in_port ((struct sockaddr*) client)),
            client->ss_family == AF_INET ? 4 : 6);
    node->next = NULL;

    ml_arg->prev->next = node;
    ml_arg->prev = node;

    return 0;
#undef ELM_FMT
#undef ELM_STRLEN
}

/*-----------------------------------------------------------------------------
 * Constructs a JSON array from the server's list of clients, excluding the
 * client given by the supplied IP address and port number */
//-----------------------------------------------------------------------------
int clients_to_json (struct response_node **dest, struct sockaddr_storage *ign,
        const char *n)
{
#ifdef LISP_OUTPUT
#define LIST_OPEN  "("
#define LIST_CLOSE ")"
#else
#define LIST_OPEN  "["
#define LIST_CLOSE "]"
#endif
    int num;
    char *endptr;
    struct make_list_arg arg;
    struct response_node *head;

    num = strtol (n, &endptr, 10);
    if (num < 0 || *endptr != '\0')
        return CL_BADNUM;

    arg.i = 0;
    arg.n = num;
    arg.ignore = ign;

    arg.prev = malloc (sizeof (struct response_node));
    arg.prev->data = strdup (LIST_OPEN);
    arg.prev->size = 1;
    arg.prev->next = NULL;
    head = arg.prev;

    delta_foreach (&client_table, make_list, &arg);

    if (arg.prev != head)
        arg.prev->size--; // ignore trailing separator

    arg.prev->next = malloc (sizeof (struct response_node));
    arg.prev->next->data = strdup (LIST_CLOSE "\r\n\r\n");
    arg.prev->next->size = 5;
    arg.prev->next->next = NULL;

    *dest = head;
    return CL_OK;
#undef LIST_OPEN
#undef LIST_CLOSE
}

static int fwd_to_client (const void *data, void *arg)
{
    struct msg_info *mi = arg;
    udp_send_msg (mi->msg, mi->len, data);
    return 0;
}

int flood_to_clients (struct msg_info *mi)
{
    delta_foreach (&client_table, fwd_to_client, mi);
    return CL_OK;
}

unsigned int client_list_size (void)
{
    return delta_size (&client_table);
}
