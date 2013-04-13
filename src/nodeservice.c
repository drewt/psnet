#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>

#include "common.h"
#include "tcp.h"
#include "udp.h"
#include "response.h"
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

#define node_error(sock, no) send_error (sock, no, psnode_strerror[no])
enum input_errors { ENOMETHOD, EBADMETHOD };
static const char *psnode_strerror[] = {
    [ENOMETHOD]  = "no method given",
    [EBADMETHOD] = "unrecognized method"
};

/*-----------------------------------------------------------------------------
 * Process an info request */
//-----------------------------------------------------------------------------
static void process_info (struct conn_info *ci, const char *msg, jsmntok_t *tok,
        size_t ntok)
{
    struct response_node head, body;

    // TODO
    make_simple_response (&body, "{\"name\":\"myname\"}", 17);
    make_response_with_body (&head, body.next);
    send_response (ci->sock, head.next);
    free_response (head.next);
}

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

#ifdef PSNETLOG
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

#ifdef PSNETLOG
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

#ifdef PSNETLOG
    printf ("%s", msg);
    printf (ANSI_YELLOW "F %s\n" ANSI_RESET, msgid);
#endif
}

static int parse_message (const char *msg, jsmntok_t *tok, size_t *ntok)
{
    int rc;
    jsmn_parser p;

    jsmn_init (&p);
    rc = jsmn_parse (&p, msg, tok, *ntok);
    if (rc != JSMN_SUCCESS || tok[0].type != JSMN_OBJECT)
        return -1;
    *ntok =  p.toknext;
    return jsmn_get_value (msg, tok, "method");
}

/*-----------------------------------------------------------------------------
 * Handles a TCP connection (callback for tcp_server_main()) */
//-----------------------------------------------------------------------------
static void *handle_connection (void *data)
{
    struct conn_info *ci = data;
    jsmntok_t tok[256];
    size_t ntok = 256;
    int method;

    for(;;) {

        if (!tcp_read_message (ci->sock, ci->msg))
            break; // connection closed by client

        // dispatch
        if ((method = parse_message (ci->msg, tok, &ntok)) == -1)
            node_error (ci->sock, ENOMETHOD);
        else if (jsmn_tokeq (ci->msg, &tok[method], "info"))
            process_info (ci, ci->msg, tok, ntok);
        else
            node_error (ci->sock, EBADMETHOD);
    }

    // clean up
    close (ci->sock);
    pthread_mutex_lock (&tcp_threads_lock);
    tcp_threads--;
    pthread_mutex_unlock (&tcp_threads_lock);
#ifdef PSNETLOG
    printf ("D %s\n", ci->paddr);
#endif
    free (ci);
    pthread_exit (NULL);
}

/*-----------------------------------------------------------------------------
 * Handles a UDP message (callback for udp_server_main()) */
//-----------------------------------------------------------------------------
static void *handle_message (void *data)
{
    struct msg_info *mi = data;
    jsmntok_t tok[256];
    size_t ntok = 256;
    int method;

    // dispatch
    if ((method = parse_message (mi->msg, tok, &ntok)) == -1)
        goto cleanup;
    else if (jsmn_tokeq (mi->msg, &tok[method], "connect"))
        process_connect (mi, tok, ntok);
    else if (jsmn_tokeq (mi->msg, &tok[method], "broadcast"))
        process_broadcast (mi, tok, ntok);
    else if (jsmn_tokeq (mi->msg, &tok[method], "ping"))
        process_ping (mi, tok, ntok);

cleanup:
    pthread_mutex_lock (&udp_threads_lock);
    udp_threads--;
    pthread_mutex_unlock (&udp_threads_lock);
    free (mi);
#ifdef PSNETLOG
    printf ("-M %s\n", mi->paddr);
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

static void *udp_serve (void *data)
{
    int sockfd;

    if (pthread_detach (pthread_self ()))
        perror ("pthread_detach");

    sockfd = udp_server_init (data);
    udp_server_main (sockfd, 10000, handle_message);
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
    pthread_t tid;

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

    if (pthread_create (&tid, NULL, udp_serve, argv[1]))
        perror ("pthread_create");

    sockfd = tcp_server_init (argv[1]);
    tcp_server_main (sockfd, settings.max_threads, handle_connection);
}
