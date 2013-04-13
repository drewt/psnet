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
        if (sockaddr_equals ((struct sockaddr*)it, (struct sockaddr*)&mi->addr))
            continue;
        udp_send_msg (mi->msg, mi->len, (struct sockaddr*) &it->addr);
    }

    // send message to clients
    flood_to_clients (mi);

    pthread_mutex_unlock (&routers_lock);
}

void routers_to_json (struct response_node **dst, int n)
{
#ifdef LISP_OUTPUT
#define LIST_OPEN  "("
#define LIST_CLOSE ")"
#define ELM_FMT "(:ip \"%s\" :port %d :ipv %d) " // 23
#define MAX_LEN INET6_ADDRSTRLEN + PORT_STRLEN + 23
#else
#define LIST_OPEN  "["
#define LIST_CLOSE "]"
#define ELM_FMT "{\"ip\":\"%s\",\"port\":%d,\"ipv\":%d}," //26
#define MAX_LEN INET6_ADDRSTRLEN + PORT_STRLEN + 26
#endif
    struct response_node *prev, *tmp;
    struct node_list *it;
    char addr[INET6_ADDRSTRLEN];

    *dst = malloc (sizeof (struct response_node));
    (*dst)->data = strdup (LIST_OPEN);
    (*dst)->size = 1;
    (*dst)->next = NULL;
    prev = *dst;

    pthread_mutex_lock (&routers_lock);

    for (it = routers.next; it; it = it->next) {
        tmp = malloc (sizeof (struct response_node));
        inet_ntop (it->addr.ss_family,
                get_in_addr ((struct sockaddr*) &it->addr), addr, sizeof addr);
        tmp->data = malloc (MAX_LEN);
        tmp->size = snprintf (tmp->data, MAX_LEN, ELM_FMT, addr,
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
    tmp->data = strdup (LIST_CLOSE);
    tmp->size = 1;
    tmp->next = NULL;
    prev->next = tmp;
#undef LIST_OPEN
#undef LIST_CLOSE
#undef ELM_FMT
#undef MAX_LEN
}
