#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "common.h"
#include "udp.h"
#include "router.h"
#include "nodelist.h"
#include "dirclient.h"

#define DIR_ADDR "127.0.0.1"
#define DIR_PORT "6666"

#define OUTDEGREE 32

static struct node_list routers;
static pthread_mutex_t routers_lock;

static void __attribute((noreturn)) *router_update_thread (void *data)
{
    char *port = data;

    for(;;) {
        for (;;) {
            pthread_mutex_lock (&routers_lock);
            if (!dir_discover (&routers, DIR_ADDR, DIR_PORT, port, OUTDEGREE))
                break;
            fprintf (stderr, "get_list: failed to update router list\n");
            pthread_mutex_unlock (&routers_lock);
            sleep (DIR_RETRY_INTERVAL);
        }
        for (struct node_list *it = routers.next; it; it = it->next) {
            char s[INET6_ADDRSTRLEN];
            inet_ntop (it->addr.ss_family,
                    get_in_addr ((struct sockaddr*) &it->addr),
                    s, sizeof s);
            printf ("node: %s:%d\n", s,
                    get_in_port ((struct sockaddr*) &it->addr));
        }
        pthread_mutex_unlock (&routers_lock);
        sleep (ROUTERS_UPDATE_INTERVAL);
    }
}

static void __attribute((noreturn)) *router_keepalive_thread (void *data)
{
    int status;
    char *port = data;

    for(;;) {
        if (dir_connect (DIR_ADDR, DIR_PORT, port, &status) == -1)
            fprintf (stderr, "send_connect: failed to update directory\n");
        else if (status != STATUS_OKAY)
            fprintf (stderr, "send_connect: directory returned error %d\n",
                    status);
        sleep (DIR_KEEPALIVE_INTERVAL);
    }
}

void router_init (char *listen_port)
{
    pthread_t tid;

    routers.next = NULL;
    pthread_mutex_init (&routers_lock, NULL);
    if (pthread_create (&tid, NULL, router_update_thread, listen_port))
        perror ("pthread_create");
    if (pthread_create (&tid, NULL, router_keepalive_thread, listen_port))
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
        udp_send_msg (mi->msg, mi->len, &it->addr);
    }

    // send message to clients


    pthread_mutex_unlock (&routers_lock);
}
