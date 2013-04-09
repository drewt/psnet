#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "common.h"
#include "udp.h"
#include "ctable.h"
#include "router.h"

#define JSMN_STRICT
#include "jsmn.h"

#define MAX_HOPS 4

/*static int udp_send_msg (const char *msg, size_t msg_len,
        struct sockaddr_storage *dst)
{
    int s;
    socklen_t ss_len;

    if ((s = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        perror ("socket");
        return;
    }

    ss_len = sizeof (*dst);
    if (sendto (s, msg, msg_len, 0, (struct sockaddr*) dst, ss_len) == -1)
        perror ("sendto");
}*/

static void process_connect (struct msg_info *mi, jsmntok_t *tok, int ntok)
{
    int port;
    long lport;

    if ((port = jsmn_get_value (mi->msg, tok, "port")) == -1)
        return;

    lport = strtol (mi->msg+tok[port].start, NULL, 10);
    if (lport < PORT_MIN || lport > PORT_MAX)
        return;

    set_in_port ((struct sockaddr*)&mi->addr, (in_port_t) lport);

    // add client to ctable

}

static void process_search (struct msg_info *mi, jsmntok_t *tok, int ntok)
{
    struct sockaddr_storage ss;
    int hops, hop_port;
    char *msg = mi->msg;
    char v;
    long lport;
    
    if ((hops = jsmn_get_value (msg, tok, "hops")) == -1)
        return;

    if ((hop_port = jsmn_get_value (msg, tok, "hop-port")) == -1)
        return;

    v = msg[tok[hops].start];
    if (v < '0' || v > '0' + MAX_HOPS)
        return;
    msg[tok[hops].start]++;

    lport = strtol (msg+tok[hop_port].start, NULL, 10);
    if (lport < PORT_MIN || lport > PORT_MAX)
        return;

    ss = mi->addr;
    if (ss.ss_family == AF_INET)
        ((struct sockaddr_in*)&ss)->sin_port = (in_port_t) lport;
    else
        ((struct sockaddr_in6*)&ss)->sin6_port = (in_port_t) lport;

    flood_message (mi);

    printf ("hops: %.*s\nhop-port: %.*s\nmsg: %s\n",
            jsmn_toklen (&tok[hops]), msg+tok[hops].start,
            jsmn_toklen (&tok[hop_port]), msg+tok[hop_port].start,
            msg);
}

static void *handle_message (void *data)
{
    struct msg_info *msg = data;
    jsmn_parser p;
    jsmntok_t tok[256];
    int rc;

    // parse message
    jsmn_init (&p);
    rc = jsmn_parse (&p, msg->msg, tok, 256);
    if (rc != JSMN_SUCCESS || tok[0].type != JSMN_OBJECT)
        goto cleanup;

    // determine method
    int method = jsmn_get_value (msg->msg, tok, "method");
    if (method == -1)
        goto cleanup;

    // dispatch
    if (jsmn_tokeq (msg->msg, &tok[method], "connect"))
        process_connect (msg, tok, p.toknext);
    else if (jsmn_tokeq (msg->msg, &tok[method], "search"))
        process_search (msg, tok, p.toknext);

cleanup:
    pthread_mutex_lock (&udp_threads_lock);
    udp_threads--;
    pthread_mutex_unlock (&udp_threads_lock);
    free (msg);
    pthread_exit (NULL);
}

static void __attribute((noreturn)) usage (void)
{
    puts ("usage: infranode [nclients] [port]\n"
          "\twhere 'nclients' is the maximum number of threads\n"
          "\tand 'port' is the port number to listen on");
    exit (EXIT_FAILURE);
}

int main (int argc, char *argv[])
{
    char *endptr;
    int sockfd;
    int max_threads;

    if (argc != 3)
        usage ();

    endptr = NULL;
    max_threads = strtol (argv[1], &endptr, 10);
    if (max_threads < 1 || (endptr && *endptr != '\0')) {
        fprintf (stderr, "error: 'nclients' must be a positive integer\n");
        usage ();
    }

    endptr = NULL;
    if (strtol (argv[2], &endptr, 10) < 1 || (endptr && *endptr != '\0')) {
        fprintf (stderr, "error: 'port' must be a positive integer\n");
        usage ();
    }

    ctable_init ();
    router_init ();
    sockfd = udp_server_init (argv[2]);

    udp_server_main (sockfd, max_threads, handle_message);
}
