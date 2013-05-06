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

#include "common.h"
#include "router.h"
#include "udp.h"
#include "client.h"
#include "nodelist.h"
#include "response.h"
#include "dirclient.h"

#define OUTDEGREE 32

static struct node_list routers;
static pthread_mutex_t routers_lock;

struct dir_arg {
    char *addr;
    char *port;
    char *listen;
};

static _Noreturn void *router_update_thread (void *data)
{
    struct dir_arg *a = data;

    pthread_detach (pthread_self ());

    for(;;) {
        for (;;) {
            pthread_mutex_lock (&routers_lock);
            if (!dir_discover (&routers, a->addr, a->port, a->listen,
                        OUTDEGREE))
                break;
            fprintf (stderr, "get_list: failed to update router list\n");
            pthread_mutex_unlock (&routers_lock);
            sleep (DIR_RETRY_INTERVAL);
        }
#ifdef PSNETLOG
        for (struct node_list *it = routers.next; it; it = it->next) {
            printf ("U %s %d\n", it->paddr,
                    ntohs (get_in_port ((struct sockaddr*) &it->addr)));
        }
#endif
        pthread_mutex_unlock (&routers_lock);
        sleep (ROUTERS_UPDATE_INTERVAL);
    }
}

static _Noreturn void *router_keepalive_thread (void *data)
{
    struct dir_arg *a = data;

    pthread_detach (pthread_self ());

    for(;;) {
        if (dir_connect (a->addr, a->port, a->listen) == -1)
            fprintf (stderr, "send_connect: failed to update directory\n");
        sleep (DIR_KEEPALIVE_INTERVAL);
    }
}

void router_init (char *dir_addr, char *dir_port, char *listen_port)
{
    pthread_t tid;
    struct dir_arg *arg = malloc (sizeof (struct dir_arg));
    *arg = (struct dir_arg) { dir_addr, dir_port, listen_port };

    routers.next = NULL;
    pthread_mutex_init (&routers_lock, NULL);
    if (pthread_create (&tid, NULL, router_update_thread, arg))
        perror ("pthread_create");
    if (pthread_create (&tid, NULL, router_keepalive_thread, arg))
        perror ("pthread_create");
}

void flood_message (struct msg_info *mi)
{
    struct node_list *it;

    pthread_mutex_lock (&routers_lock);

    // send message to routers
    for (it = routers.next; it; it = it->next) {
        if (ip_addr_equals ((struct sockaddr*)it, (struct sockaddr*)&mi->addr))
            continue;
        udp_send_msg (mi->msg, mi->len, (struct sockaddr*) &it->addr);
    }

    // send message to clients
    flood_to_clients (mi);

    pthread_mutex_unlock (&routers_lock);
}

void routers_to_json (struct response_node **dst, int n)
{
#define ELM_FMT "{\"ip\":\"%s\",\"port\":%d,\"ipv\":%d},"
#define ELM_STRLEN 26 + INET6_ADDRSTRLEN + PORT_STRLEN
    struct response_node *prev, *tmp;
    struct node_list *it;
    char addr[INET6_ADDRSTRLEN];

    *dst = malloc (sizeof (struct response_node));
    (*dst)->data = strdup ("[");
    (*dst)->size = 1;
    (*dst)->next = NULL;
    prev = *dst;

    pthread_mutex_lock (&routers_lock);

    for (it = routers.next; it; it = it->next) {
        tmp = malloc (sizeof (struct response_node));
        inet_ntop (it->addr.ss_family,
                get_in_addr ((struct sockaddr*) &it->addr), addr, sizeof addr);
        tmp->data = malloc (ELM_STRLEN);
        tmp->size = snprintf (tmp->data, ELM_STRLEN, ELM_FMT, addr,
                ntohs (get_in_port ((struct sockaddr*) &it->addr)),
                it->addr.ss_family == AF_INET ? 4 : 6);
        tmp->next = NULL;

        prev->next = tmp;
        prev = tmp;
    }
    
    pthread_mutex_unlock (&routers_lock);

    if (it != routers.next)
        prev->size--; // ignore trailing separator
    tmp = malloc (sizeof (struct response_node));
    tmp->data = strdup ("]\r\n\r\n");
    tmp->size = 5;
    tmp->next = NULL;
    prev->next = tmp;
#undef ELM_FMT
#undef MAX_LEN
}
