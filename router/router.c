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
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "client.h"
#include "network.h"
#include "protocol.h"

#include "router.h"

#define OUTDEGREE 32

static LIST_HEAD(routers);
static pthread_mutex_t routers_lock;

struct tracker_arg {
	PSNET *tracker;
	in_port_t port;
};

static void set_routers(struct list_head *new)
{
	struct list_head *pos, *n;

	list_for_each_safe(pos, n, &routers) {
		list_del(pos);
		free(pos);
	}

	if (list_empty(new)) {
		INIT_LIST_HEAD(&routers);
		return;
	}

	routers.next = new->next;
	routers.prev = new->prev;
	routers.next->prev = &routers;
	routers.prev->next = &routers;
}

static void print_routers(struct list_head *head)
{
	struct list_head *pos;
	char addr[INET6_ADDRSTRLEN];

	list_for_each(pos, head) {
		PSNET *ent = (PSNET*) &((struct psnet_list_entry*)pos)->addr;
		psnet_ntop(ent, addr);
		printf("U %s %d\n", addr, psnet_get_port(ent));
	}
}

static _Noreturn void *router_update_thread(void *data)
{
	struct list_head tmp;
	struct tracker_arg *a = data;

	pthread_detach(pthread_self());

	for(;;) {
		INIT_LIST_HEAD(&tmp);
		for (;;) {
			pthread_mutex_lock(&routers_lock);
			if (!psnet_request_discover(a->tracker, &tmp, OUTDEGREE,
					a->port))
			break;
			fprintf(stderr, "get_list: failed to update router list\n");
			pthread_mutex_unlock(&routers_lock);
			sleep(DIR_RETRY_INTERVAL);
		}

		set_routers(&tmp);
#ifdef PSNETLOG
		print_routers(&routers);
#endif
		pthread_mutex_unlock(&routers_lock);
		sleep(ROUTERS_UPDATE_INTERVAL);
	}
}

static _Noreturn void *router_keepalive_thread(void *data)
{
	struct tracker_arg *a = data;

	pthread_detach(pthread_self());

	for(;;) {
		if (psnet_send_connect(a->tracker, a->port) == -1)
			fprintf(stderr, "send_connect: failed to update tracker\n");
		sleep(DIR_KEEPALIVE_INTERVAL);
	}
}

int router_init(char *tracker_addr, char *tracker_port, char *listen_port)
{
	struct tracker_arg *arg;
	pthread_t tid;
	PSNET *tracker;
	in_port_t port;

	tracker = psnet_new(tracker_addr, tracker_port);
	if (tracker == NULL)
		return -1;

	port =(in_port_t) atoi(listen_port);
	if (port == 0)
		return -1;

	arg = malloc(sizeof(struct tracker_arg));
	arg->tracker = tracker;
	arg->port = port;

	pthread_mutex_init(&routers_lock, NULL);
	if (pthread_create(&tid, NULL, router_update_thread, arg))
		perror("pthread_create");
	if (pthread_create(&tid, NULL, router_keepalive_thread, arg))
		perror("pthread_create");

	return 0;
}

void flood_message(struct msg_info *mi)
{
	struct list_head *it;

	pthread_mutex_lock(&routers_lock);

	// send message to routers
	list_for_each(it, &routers) {
		struct sockaddr *addr = psnet_list_entry_addr(it);
		if (ip_addr_equals(addr,(struct sockaddr*)&mi->addr))
			continue;
		udp_send(addr, mi->len, mi->msg);
	}

	// send message to clients
	flood_to_clients(mi);

	pthread_mutex_unlock(&routers_lock);
}

void routers_to_json(struct list_head *head, int n)
{
#define ELM_FMT "{\"ip\":\"%s\",\"port\":%d,\"ipv\":%d},"
#define ELM_STRLEN 26 + INET6_ADDRSTRLEN + PORT_STRLEN
	struct list_head *it;
	struct response_node *node;
	char addr[INET6_ADDRSTRLEN];

	node = malloc(sizeof(struct response_node));
	node->data = strdup("[");
	node->len = 1;
	list_add_tail((struct list_head*)node, head);

	pthread_mutex_lock(&routers_lock);

	list_for_each(it, &routers) {
		struct psnet_list_entry *entry = (struct psnet_list_entry*)it;
		node = malloc(sizeof(struct response_node));
		inet_ntop(entry->addr.ss_family,
				get_in_addr((struct sockaddr*) &entry->addr),
				addr, sizeof addr);
		node->data = malloc(ELM_STRLEN);
		node->len = snprintf(node->data, ELM_STRLEN, ELM_FMT, addr,
				ntohs(get_in_port((struct sockaddr*) &entry->addr)),
				entry->addr.ss_family == AF_INET ? 4 : 6);
		list_add_tail((struct list_head*)node, head);
	}
    
	pthread_mutex_unlock(&routers_lock);

	/* ignore trailing separator */
	if (head->prev != head->next)
		((struct response_node*)head->prev)->len--;

	node = malloc(sizeof(struct response_node));
	node->data = strdup("]\r\n\r\n");
	node->len = 5;
	list_add_tail((struct list_head*)node, head);
#undef ELM_FMT
#undef ELM_STRLEN
}
