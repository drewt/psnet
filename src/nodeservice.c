#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>

#include "common.h"
#include "udp.h"
#include "client.h"
#include "router.h"
#include "msgcache.h"

#define JSMN_STRICT
#include "jsmn.h"

#define MAX_HOPS 4

#define PONG_MAX (25 + PORT_STRLEN)

static struct {
    char *str;
    size_t len;
} udp_port;

/*-----------------------------------------------------------------------------
 * Process a 'ping' packet: send a pong */
//-----------------------------------------------------------------------------
static void process_ping (struct msg_info *mi, jsmntok_t *tok, int ntok)
{
    int port;
    long lport;

    if ((port = jsmn_get_value (mi->msg, tok, "port")) == -1)
        return;

    lport = strtol (mi->msg + tok[port].start, NULL, 10);
    if (lport < PORT_MIN || lport > PORT_MAX)
        return;

    set_in_port ((struct sockaddr*)&mi->addr, htons((in_port_t) lport));

#ifdef P2PSERV_LOG
    printf (ANSI_YELLOW "P %s %d\n" ANSI_RESET, mi->paddr,
            get_in_port ((struct sockaddr*)&mi->addr));
#endif

    udp_send_msg ("{\"method\":\"pong\"}", 17, &mi->addr);
}

/*-----------------------------------------------------------------------------
 * Processes a keep-alive message */
//-----------------------------------------------------------------------------
static void process_connect (struct msg_info *mi, jsmntok_t *tok, int ntok)
{
    int port;

    if ((port = jsmn_get_value (mi->msg, tok, "port")) == -1)
        return;

    mi->msg[tok[port].end] = '\0';

    if (add_client (&mi->addr, mi->msg + tok[port].start))
        return;

#ifdef P2PSERV_LOG
    printf (ANSI_GREEN "+ %s %s\n" ANSI_RESET, mi->paddr,
            mi->msg + tok[port].start);
#endif
}

/*-----------------------------------------------------------------------------
 * Processes a search query: increments the 'hops' field (discarding the
 * message if it's reached the hop limit) and forwards the message to all known
 * routers and clients */
//-----------------------------------------------------------------------------
static void process_broadcast (struct msg_info *mi, jsmntok_t *tok, int ntok)
{
    int hops, hop_port, id;
    char *msg = mi->msg;
    char *msgid;
    char v;
    long lport;

    if ((hops = jsmn_get_value (msg, tok, "hops")) == -1)
        return;

    if ((hop_port = jsmn_get_value (msg, tok, "hop-port")) == -1)
        return;

    if ((id = jsmn_get_value (msg, tok, "id")) == -1)
        return;

    v = msg[tok[hops].start];
    if (v < '0' || v >= '0' + MAX_HOPS - 1)
        return;
    msg[tok[hops].start]++;

    lport = strtol (msg+tok[hop_port].start, NULL, 10);
    if (lport < PORT_MIN || lport > PORT_MAX)
        return;

    msgid = jsmn_tokdup (msg, &tok[id]);
    if (cache_msg (msgid)) {
        free (msgid);
        return;
    }

    set_in_port ((struct sockaddr*) &mi->addr, htons ((in_port_t) lport));

    // set hop-port to our listen port
    // XXX: hop-port should be a string of length PORT_STRLEN so there is space
    memset (msg+tok[hop_port].start, ' ', PORT_STRLEN);
    strncpy (msg+tok[hop_port].start, udp_port.str, udp_port.len);

    flood_message (mi);

#ifdef P2PSERV_LOG
    printf (ANSI_YELLOW "F %s\n" ANSI_RESET, msgid);
#endif
}

/*-----------------------------------------------------------------------------
 * Handles a UDP message (callback for udp_server_main()) */
//-----------------------------------------------------------------------------
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
    else if (jsmn_tokeq (msg->msg, &tok[method], "broadcast"))
        process_broadcast (msg, tok, p.toknext);
    else if (jsmn_tokeq (msg->msg, &tok[method], "ping"))
        process_ping (msg, tok, p.toknext);
    else
        printf ("junk packet: %s\n", msg->msg);

cleanup:
    pthread_mutex_lock (&udp_threads_lock);
    udp_threads--;
    pthread_mutex_unlock (&udp_threads_lock);
    free (msg);
#ifdef P2PSERV_LOG
    printf ("-M %s\n", msg->paddr);
#endif
    pthread_exit (NULL);
}

/*-----------------------------------------------------------------------------
 * Usage... */
//-----------------------------------------------------------------------------
static _Noreturn void usage (void)
{
    puts ("usage: infranode [nclients] [port]\n"
          "       where 'nclients' is the maximum number of threads\n"
          "       and 'port' is the port number to listen on");
    exit (EXIT_FAILURE);
}

/*-----------------------------------------------------------------------------
 * Main... */
//-----------------------------------------------------------------------------
int main (int argc, char *argv[])
{
    char *endptr;
    int sockfd;
    int max_threads;
    long lport;

    if (argc != 3)
        usage ();

    endptr = NULL;
    max_threads = strtol (argv[1], &endptr, 10);
    if (max_threads < 1 || (endptr && *endptr != '\0')) {
        fprintf (stderr, "error: 'nclients' must be a positive integer\n");
        usage ();
    }

    endptr = NULL;
    lport = strtol (argv[2], &endptr, 10);
    if (lport < PORT_MIN || lport > PORT_MAX || (endptr && *endptr != '\0')) {
        fprintf (stderr, "error: invalid port\n");
        usage ();
    }
    udp_port.str = argv[2];
    udp_port.len = strlen (udp_port.str);

    clients_init ();
    msg_cache_init ();
    router_init (udp_port.str);

    sockfd = udp_server_init (argv[2]);
    udp_server_main (sockfd, max_threads, handle_message);
}
