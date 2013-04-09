#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "common.h"
#include "udp.h"
#include "router.h"
#include "dirclient.h"

#define DIR_ADDR "127.0.0.1"
#define DIR_PORT "6666"

static struct node_list routers_head;
static pthread_mutex_t routers_lock;

static void __attribute((noreturn)) *router_update_thread (void *data)
{
    for(;;) {
        for (;;) {
            pthread_mutex_lock (&routers_lock);
            if (!dir_get_list (&routers_head, DIR_ADDR, DIR_PORT))
                break;
            fprintf (stderr, "get_list: failed to update router list\n");
            pthread_mutex_unlock (&routers_lock);
            sleep (DIR_RETRY_INTERVAL);
        }
        for (struct node_list *it = routers_head.next; it; it = it->next) {
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

    for(;;) {
        if (dir_send_connect (DIR_ADDR, DIR_PORT, &status) == -1)
            fprintf (stderr, "send_connect: failed to update directory\n");
        else if (status != STATUS_OKAY)
            fprintf (stderr, "send_connect: directory returned error %d\n",
                    status);
        sleep (DIR_KEEPALIVE_INTERVAL);
    }
}

void router_init (void)
{
    pthread_t tid;

    pthread_mutex_init (&routers_lock, NULL);
    if (pthread_create (&tid, NULL, router_update_thread, NULL))
        perror ("pthread_create");
    if (pthread_create (&tid, NULL, router_keepalive_thread, NULL))
        perror ("pthread_create");
}

static void send_message (struct msg_info *mi, struct sockaddr_storage *dst)
{
    int s;
    socklen_t ss_len;

    if ((s = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        perror ("socket");
        return;
    }

    ss_len = sizeof (*dst);
    if (sendto (s, mi->msg, mi->len, 0, (struct sockaddr*) dst, ss_len) == -1)
        perror ("sendto");
}

void flood_message (struct msg_info *mi)
{
    struct node_list *it;

    pthread_mutex_lock (&routers_lock);

    // send message to routers
    for (it = routers_head.next; it; it = it->next) {
        if (sockaddr_equals ((struct sockaddr*)it, (struct sockaddr*)&mi->addr))
            continue;
        send_message (mi, &it->addr);
    }

    // send message to clients


    pthread_mutex_unlock (&routers_lock);
}
