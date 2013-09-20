/* Copyright 2013 Drew Thoreson */

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

#include "client.h"
#include "deltalist.h"
#include "ipv6.h"
#include "misc.h"
#include "network.h"
#include "types.h"

static unsigned long delta_hash(const void *client);
static int delta_equals(const void *a, const void *b);
static void delta_act(const void *client);

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

static unsigned long delta_hash(const void *data)
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

static int delta_equals(const void *a, const void *b)
{
	return sockaddr_equals(a, b);
}

static void delta_act(const void *data)
{
#ifdef PSNETLOG
	const struct sockaddr_storage *client = data;
	char addr[INET6_ADDRSTRLEN];
	inet_ntop(client->ss_family, get_in_addr((struct sockaddr*) client),
			addr, sizeof addr);
	printf(ANSI_RED "X %s %d\n" ANSI_RESET, addr,
			ntohs(get_in_port((struct sockaddr*) client)));
	fflush(stdout);
#endif
}

void clients_init(void)
{
	delta_init(&client_table);
}

static int make_client(struct sockaddr_storage *addr, const char *port)
{
	char *endptr;
	long lport;

	lport = strtol(port, &endptr, 10);
	if (lport < PORT_MIN || lport > PORT_MAX || *endptr != '\0')
		return -1;

	set_in_port((struct sockaddr*) addr, htons((in_port_t) lport));
	return 0;
}

/*-----------------------------------------------------------------------------
 * Adds a client to the network */
//-----------------------------------------------------------------------------
int add_client(struct sockaddr_storage *addr, const char *port)
{
	struct sockaddr_storage *client;

	client = malloc(sizeof(struct sockaddr_storage));
	*client = *addr;

	if (make_client(client, port))
		return -1;

	delta_update(&client_table, client);
	return 0;
}

/*-----------------------------------------------------------------------------
 * Removes a client from the network */
//-----------------------------------------------------------------------------
int remove_client(struct sockaddr_storage *addr, const char *port)
{
	struct sockaddr_storage client = *addr;

	if (make_client(&client, port))
		return -1;

	if (delta_remove(&client_table, &client))
		return -1;
	return 0;
}

/* argument to the make_list() function */
struct make_list_arg {
	struct list_head *list;
	struct sockaddr_storage *ignore;
	int i;
	int n;
};

/*-----------------------------------------------------------------------------
 * Constructs a JSON representation of the given client structure and inserts
 * it into the list given in the argument */
//-----------------------------------------------------------------------------
static int make_list(const void *data, void *arg)
{
	const struct sockaddr_storage *client = data;
	struct make_list_arg *ml_arg = arg;
	if (ml_arg->i >= ml_arg->n)
		return 1;
	if (ml_arg->ignore && delta_equals(client, ml_arg->ignore))
		return 0;
	ml_arg->i++;

	char addr[INET6_ADDRSTRLEN];
	inet_ntop (client->ss_family, get_in_addr((struct sockaddr*) client),
			addr, sizeof addr);

	struct response_node *node = malloc(sizeof(struct response_node));

	node->data = malloc(25 + INET6_ADDRSTRLEN + PORT_STRLEN + 1);
	node->len = sprintf(node->data,
			"{\"ip\":\"%s\",\"port\":%d,\"ipv\":%d},",
			addr,
			ntohs (get_in_port((struct sockaddr*) client)),
			client->ss_family == AF_INET ? 4 : 6);
	list_add_tail(&node->chain, ml_arg->list);

	return 0;
}

/*-----------------------------------------------------------------------------
 * Constructs a JSON array from the server's list of clients, excluding the
 * client given by the supplied sockaddr structure */
//-----------------------------------------------------------------------------
int clients_to_json(struct list_head *head, struct sockaddr_storage *ign,
        const char *n)
{
	int num;
	char *endptr;
	struct make_list_arg arg;
	struct response_node *first, *last;

	num = strtol(n, &endptr, 10);
	if (num < 0 || *endptr != '\0')
		return CL_BADNUM;

	arg.i = 0;
	arg.n = num;
	arg.list = head;
	arg.ignore = ign;

	first = malloc(sizeof(struct response_node));
	first->data = strdup("[");
	first->len = 1;
	list_add_tail((struct list_head*)first, head);

	delta_foreach (&client_table, make_list, &arg);

	/* ignore trailing separator */
	if (head->prev != head->next)
		((struct response_node*)head->prev)->len--;

	last = malloc(sizeof(struct response_node));
	last->data = strdup("]\r\n\r\n");
	last->len = 5;
	list_add_tail((struct list_head*)last, head);

	return CL_OK;
}

static int fwd_to_client(const void *data, void *arg)
{
	struct msg_info *mi = arg;
	udp_send(data, mi->len, mi->msg);
	return 0;
}

int flood_to_clients(struct msg_info *mi)
{
	delta_foreach(&client_table, fwd_to_client, mi);
	return CL_OK;
}

unsigned int client_list_size(void)
{
	return delta_size(&client_table);
}
