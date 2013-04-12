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
#include "ini.h"

#define MAX_HOPS 4

#define PONG_MAX (25 + PORT_STRLEN)

static struct {
    char *dir_addr;
    char *dir_port;
    char *listen_port;
    in_port_t max_threads;
} settings = {
    .dir_addr = "127.0.0.1",
    .dir_port = "6666",
    .listen_port = "5555",
    .max_threads = 10000
};

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

    udp_send_msg ("{\"method\":\"pong\"}", 17, (struct sockaddr*) &mi->addr);
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
    char *s, *d;
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
    for (d = msg + tok[hop_port].start, s = settings.listen_port;
            *s != '\0'; *d++ = *s++);
    while (*d != '"') *d++ = ' ';

    flood_message (mi);

#ifdef P2PSERV_LOG
    printf ("%s", msg);
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
    puts ("usage: infranode [port]\n"
          "       where 'port' is the port number to listen on");
    exit (EXIT_FAILURE);
}

static int ini_handler (void *user, const char *section, const char *name,
        const char *value)
{
    if (!strcmp (name, "dir_addr")) {
        settings.dir_addr = strdup (value);
    } else if (!strcmp (name, "dir_port")) {
        settings.dir_port = strdup (value);
    } else if (!strcmp (name, "udp_port")) {
        settings.listen_port = strdup (value);
    } else if (!strcmp (name, "max_threads")) {
        long lport = strtol (value, NULL, 10);
        if (lport < PORT_MIN || lport > PORT_MAX)
            return 0;
        settings.max_threads = (in_port_t) lport;
    }
    return 1;
}

/*-----------------------------------------------------------------------------
 * Main... */
//-----------------------------------------------------------------------------
int main (int argc, char *argv[])
{
    char *home, *path;
    char *endptr;
    int sockfd;
    long lport;

    if (argc != 2)
        usage ();

    // read the port number from argv
    endptr = NULL;
    lport = strtol (argv[1], &endptr, 10);
    if (lport < PORT_MIN || lport > PORT_MAX || (endptr && *endptr != '\0')) {
        fprintf (stderr, "error: invalid port\n");
        usage ();
    }
    settings.listen_port= argv[1];

    // read additional settings from configuration file
    if (!ini_parse ("psnoderc", ini_handler, NULL))
        goto init;

    home = getenv ("HOME");
    path = malloc (strlen (home) + 10);
    sprintf (path, "%s%s", home, ".psnoderc");
    if (!(lport = ini_parse (path, ini_handler, NULL)))
        goto init;

    if (!ini_parse ("/etc/psnoderc", ini_handler, NULL))
        goto init;

    fprintf (stderr, "error: failed to read psnoderc\n");

init:
    clients_init ();
    msg_cache_init ();
    router_init (settings.dir_addr, settings.dir_port, settings.listen_port);

    sockfd = udp_server_init (argv[1]);
    udp_server_main (sockfd, settings.max_threads, handle_message);
}
