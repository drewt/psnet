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
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>

#define JSMN_STRICT
#include "jsmn.h"
#include "ini.h"

#include "common.h"
#include "tcp.h"
#include "udp.h"
#include "response.h"
#include "client.h"
#include "router.h"
#include "msgcache.h"

#define MAX_HOPS 4

#ifdef LISP_OUTPUT
#define HDR_OK_FMT "(:status \"okay\" :size %d)\r\n\r\n"
#define HDR_OK_STRLEN (27 + 5)
#else
#define HDR_OK_FMT "{\"status\":\"okay\",\"size\":%d}\r\n\r\n"
#define HDR_OK_STRLEN (29 + 5)
#endif

static struct {
    char *dir_addr;
    char *dir_port;
    char *listen_port;
} settings = {
    .dir_addr = "psnet.no-ip.biz",
    .dir_port = "6666",
    .listen_port = "5555",
};

#define node_error(sock, no) send_error (sock, no, psnode_strerror[no])
enum input_errors { ENOMETHOD, ENONUM, EBADMETHOD, EBADNUM };
static const char *psnode_strerror[] = {
    [ENOMETHOD]  = "no method given",
    [ENONUM]     = "missing argument 'num'",
    [EBADMETHOD] = "unrecognized method",
    [EBADNUM]    = "invalid argument 'num'"
};

int max_threads = 1000;
int num_threads;
pthread_mutex_t num_threads_lock;

/*-----------------------------------------------------------------------------
 * Process an info request */
//-----------------------------------------------------------------------------
static void process_ip (struct msg_info *mi, jsmntok_t *tok, size_t ntok)
{
#ifdef LISP_OUTPUT
#define RSP_FMT "(:ip \"%s\")\r\n\r\n"
#else
#define RSP_FMT "{\"ip\":\"%s\"}\r\n\r\n"
#endif

    char addr[INET6_ADDRSTRLEN];
    char hdr[HDR_OK_STRLEN];
    char rsp[13 + INET6_ADDRSTRLEN];
    int hdr_len, rsp_len;

    inet_ntop (mi->addr.ss_family, get_in_addr ((struct sockaddr*) &mi->addr),
            addr, sizeof addr);
    rsp_len = sprintf (rsp, RSP_FMT, addr);
    hdr_len = sprintf (hdr, HDR_OK_FMT, rsp_len);

    tcp_send_bytes (mi->sock, hdr, hdr_len);
    tcp_send_bytes (mi->sock, rsp, rsp_len);

#undef RSP_FMT
}

/*-----------------------------------------------------------------------------
 * Process an info request */
//-----------------------------------------------------------------------------
static void process_info (struct msg_info *mi, jsmntok_t *tok, size_t ntok)
{
#ifdef LISP_OUTPUT
#define INFO_FMT "(:name \"generic psnet router\" "\
                  ":clients %d :cache-load %d)\r\n\r\n"
#define INFO_STRLEN (57 + 10 + 10)
#else
#define INFO_FMT "{\"name\":\"generic psnet router\","\
                  "\"clients\":%d,\"cache-load\":%d}\r\n\r\n"
#define INFO_STRLEN (60 + 10 + 10)
#endif

    char hdr[HDR_OK_STRLEN];
    char rsp[INFO_STRLEN];
    int hdr_len, rsp_len;

    rsp_len = sprintf (rsp, INFO_FMT, client_list_size (), msg_cache_size ());
    hdr_len = sprintf (hdr, HDR_OK_FMT, rsp_len);

    tcp_send_bytes (mi->sock, hdr, hdr_len);
    tcp_send_bytes (mi->sock, rsp, rsp_len);

#undef INFO_FMT
#undef INFO_STRLEN
}

/*-----------------------------------------------------------------------------
 * Process a 'ping' packet: send a pong */
//-----------------------------------------------------------------------------
static void process_ping (struct msg_info *mi, jsmntok_t *tok, int ntok)
{
    send_ok (mi->sock);

#ifdef PSNETLOG
    printf (ANSI_YELLOW "P %s %d\n" ANSI_RESET, mi->paddr,
            get_in_port ((struct sockaddr*)&mi->addr));
#endif
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

    if (jsmn_get_values (msg, tok, "hops", &hops, "hop-port", &hop_port,
            "id", &id, (void*) NULL) == -1)
        return;

    v = msg[tok[hops].start];
    if (v < '0' || v >= '0' + MAX_HOPS - 1)
        return; // hop limit reached
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

/*-----------------------------------------------------------------------------
 * */
//-----------------------------------------------------------------------------
static void process_discover (struct msg_info *mi, jsmntok_t *tok, int ntok)
{
    struct response_node head;
    struct response_node *jlist;
    int num;

    if ((num = jsmn_get_value (mi->msg, tok, "num")) == -1) {
        node_error (mi->sock, ENONUM);
        return;
    }

    if (!(num = atoi (mi->msg + tok[num].start))) {
        node_error (mi->sock, EBADNUM);
        return;
    }

    routers_to_json (&jlist, num);
    make_response_with_body (&head, jlist);
    send_response (mi->sock, head.next);
    free_response (head.next);

#ifdef PSNETLOG
    printf (ANSI_YELLOW "L %s\n" ANSI_RESET, mi->paddr);
#endif
}

/*-----------------------------------------------------------------------------
 * Handles a TCP connection (callback for tcp_server_main()) */
//-----------------------------------------------------------------------------
static void *handle_connection (void *data)
{
    struct msg_info *mi = data;
    jsmntok_t tok[JSMN_NTOK];
    size_t ntok = JSMN_NTOK;
    int method;

    for(;;) {

        if (!tcp_read_message (mi->sock, mi->msg))
            break; // connection closed by client

        // dispatch
        if ((method = parse_message (mi->msg, tok, &ntok)) == -1) {
            node_error (mi->sock, ENOMETHOD);
            break;
        } else if (jsmn_tokeq (mi->msg, &tok[method], "ip")) {
            process_ip (mi, tok, ntok);
        } else if (jsmn_tokeq (mi->msg, &tok[method], "info")) {
            process_info (mi, tok, ntok);
        } else if (jsmn_tokeq (mi->msg, &tok[method], "ping")) {
            process_ping (mi, tok, ntok);
        } else if (jsmn_tokeq (mi->msg, &tok[method], "discover")) {
            process_discover (mi, tok, ntok);
        } else {
            node_error (mi->sock, EBADMETHOD);
            break;
        }
    }

    // clean up
    close (mi->sock);
    pthread_mutex_lock (&num_threads_lock);
    num_threads--;
    pthread_mutex_unlock (&num_threads_lock);
#ifdef PSNETLOG
    printf ("D %s\n", mi->paddr);
#endif
    free (mi);
    pthread_exit (NULL);
}

/*-----------------------------------------------------------------------------
 * Handles a UDP message (callback for udp_server_main()) */
//-----------------------------------------------------------------------------
static void *handle_message (void *data)
{
    struct msg_info *mi = data;
    jsmntok_t tok[JSMN_NTOK];
    size_t ntok = JSMN_NTOK;
    int method;

    // dispatch
    if ((method = parse_message (mi->msg, tok, &ntok)) == -1)
        goto cleanup;
    else if (jsmn_tokeq (mi->msg, &tok[method], "connect"))
        process_connect (mi, tok, ntok);
    else if (jsmn_tokeq (mi->msg, &tok[method], "broadcast"))
        process_broadcast (mi, tok, ntok);

cleanup:
    pthread_mutex_lock (&num_threads_lock);
    num_threads--;
    pthread_mutex_unlock (&num_threads_lock);
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

/*-----------------------------------------------------------------------------
 * UDP server thread */
//-----------------------------------------------------------------------------
static void *udp_serve (void *data)
{
    int sockfd;

    pthread_detach (pthread_self ());

    sockfd = udp_server_init (data);
    udp_server_main (sockfd, handle_message);
}

/*-----------------------------------------------------------------------------
 * Callback for ini parser */
//-----------------------------------------------------------------------------
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
        if (!(max_threads = atoi (value)))
            max_threads = 1000000;
    }
    return 1;
}

/*-----------------------------------------------------------------------------
 * Attempts to read settings from an rc file at various locations */
//-----------------------------------------------------------------------------
static int read_rc (void)
{
    char *home, *path;

    if (!ini_parse ("psnoderc", ini_handler, NULL))
        return 0;

    home = getenv ("HOME");
    path = malloc (strlen (home) + 10);
    sprintf (path, "%s%s", home, ".psnoderc");
    if (!ini_parse (path, ini_handler, NULL))
        return 0;

    if (!ini_parse ("/etc/psnoderc", ini_handler, NULL))
        return 0;

    return -1;
}

/*-----------------------------------------------------------------------------
 * Main... */
//-----------------------------------------------------------------------------
int main (int argc, char *argv[])
{
    int sockfd;
    long lport;
    pthread_t tid;

    if (argc > 2)
        usage ();

    // validate port number, if given on the command line
    if (argc == 2) {
        lport = strtol (argv[1], NULL, 10);
        if (lport < PORT_MIN || lport > PORT_MAX) {
            fprintf (stderr, "error: invalid port\n");
            usage ();
        }
        settings.listen_port= argv[1];
    }

    if (read_rc () == -1)
        fprintf (stderr, "error: failed to read psnoderc\n"
                         "using defaults:\n"
                         "\tDirectory address: %s\n"
                         "\tDirectory port:    %s\n"
                         "\tListen port:       %s\n",
                         settings.dir_addr, settings.dir_port,
                         settings.listen_port);

#ifdef DAEMON
    daemonize ();
#endif

    num_threads = 0;
    pthread_mutex_init (&num_threads_lock, NULL);

    clients_init ();
    msg_cache_init ();
    router_init (settings.dir_addr, settings.dir_port, settings.listen_port);

    if (pthread_create (&tid, NULL, udp_serve, settings.listen_port))
        perror ("pthread_create");

    sockfd = tcp_server_init (settings.listen_port);
    tcp_server_main (sockfd, handle_connection);
}
