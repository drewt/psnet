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

#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
#include <getopt.h>

#include "jsmn.h"
#include "ini.h"

#include "common.h"
#include "tcp.h"
#include "udp.h"
#include "response.h"
#include "client.h"

#define RC_FILE "/etc/psnetrc"

#define HDR_OK_FMT "{\"status\":\"okay\",\"size\":%d}\r\n\r\n"
#define HDR_OK_STRLEN (29 + 5)

#define dir_error(sock, no) send_error (sock, no, psdir_strerror[no])
enum input_errors { ENOMETHOD,ENONUM,ENOPORT,EBADMETHOD,EBADNUM,EBADPORT };
static const char *psdir_strerror[] = {
    [ENOMETHOD]  = "no method given",
    [ENONUM]     = "missing argument 'num'",
    [ENOPORT]    = "missing argument 'port'",
    [EBADMETHOD] = "unrecognized method",
    [EBADNUM]    = "invalid argument 'num'",
    [EBADPORT]   = "invalid argument 'port'"
};

int num_threads;
pthread_mutex_t num_threads_lock;

static struct settings {
    int max_threads;
    char *port;
} settings = {
    .max_threads = 1000,
    .port = "6666"
};

static void process_info (struct msg_info *mi, jsmntok_t *tok, size_t ntok)
{
    char hdr[HDR_OK_STRLEN];
    char rsp[49 + 10]; /* space for 10-digit router count */
    int hdr_len, rsp_len;

    rsp_len = sprintf (rsp, "{\"name\":\"generic psnet directory\","
            "\"routers\":%d}\r\n\r\n", client_list_size());
    hdr_len = sprintf (hdr, HDR_OK_FMT, rsp_len);

    tcp_send_bytes (mi->sock, hdr, hdr_len);
    tcp_send_bytes (mi->sock, rsp, rsp_len);
}

/*-----------------------------------------------------------------------------
 * Process a 'CONNECT [port]' command */
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
 * Process a 'DISCONNECT [port]' command */
//-----------------------------------------------------------------------------
static void process_disconnect (struct msg_info *mi, jsmntok_t *tok, int ntok)
{
    int port;

    if ((port =  jsmn_get_value (mi->msg, tok, "port")) == -1)
        return;

    mi->msg[tok[port].end] = '\0';

    if (remove_client (&mi->addr, mi->msg + tok[port].start))
        return;

#ifdef PSNETLOG
    printf (ANSI_RED "- %s %s\n" ANSI_RESET, mi->paddr,
            mi->msg + tok[port].start);
#endif
}

/*-----------------------------------------------------------------------------
 * Process a 'LIST [n]' command */
//-----------------------------------------------------------------------------
static void process_list (struct msg_info *mi, jsmntok_t *tok, int ntok)
{
    struct response_node head;
    struct response_node *jlist;
    int num;
 
    if ((num = jsmn_get_value (mi->msg, tok, "num")) == -1) {
        dir_error (mi->sock, ENONUM);
        return;
    }
    mi->msg[tok[num].end] = '\0';

    if (clients_to_json (&jlist, NULL, mi->msg + tok[num].start)) {
        dir_error (mi->sock, EBADNUM);
        return;
    }

    make_response_with_body (&head, jlist);
    send_response (mi->sock, head.next);
    free_response (head.next);
#ifdef PSNETLOG
    printf (ANSI_YELLOW "L %s\n" ANSI_RESET, mi->paddr);
#endif
}

/*-----------------------------------------------------------------------------
 * Process a 'DISCOVER [port] [n]' command */
//-----------------------------------------------------------------------------
static void process_discover (struct msg_info *mi, jsmntok_t *tok, int ntok)
{
    struct response_node head;
    struct response_node *jlist;
    int num, port;
    int iport;

    if ((num = jsmn_get_value (mi->msg, tok, "num")) == -1) {
        dir_error (mi->sock, ENONUM);
        return;
    }
    mi->msg[tok[num].end] = '\0';
    if ((port = jsmn_get_value (mi->msg, tok, "port")) == -1) {
        dir_error (mi->sock, ENOPORT);
        return;
    }
    mi->msg[tok[port].end] = '\0';

    iport = atoi (mi->msg + tok[port].start);
    if (iport < PORT_MIN || iport > PORT_MAX) {
        dir_error (mi->sock, EBADPORT);
        return;
    }

    set_in_port ((struct sockaddr*)&mi->addr, htons ((in_port_t) iport));
    if (clients_to_json (&jlist, &mi->addr, mi->msg + tok[num].start)) {
        dir_error (mi->sock, EBADNUM);
        return;
    }

    make_response_with_body (&head, jlist);
    send_response (mi->sock, head.next);
    free_response (head.next);
#ifdef PSNETLOG
    printf (ANSI_YELLOW "L %s %s\n" ANSI_RESET, mi->paddr,
            mi->msg + tok[port].start);
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
        #define cmd_equal(cmd) jsmn_tokeq (mi->msg, &tok[method], cmd)
        if ((method = parse_message (mi->msg, tok, &ntok)) == -1) {
            dir_error (mi->sock, ENOMETHOD);
            break;
        } else if (cmd_equal ("list")) {
            process_list (mi, tok, ntok);
        } else if (cmd_equal ("discover")) {
            process_discover (mi, tok, ntok);
        } else if (cmd_equal ("info")) {
            process_info (mi, tok, ntok);
        } else {
            dir_error (mi->sock, EBADMETHOD);
            break;
        }
        #undef cmd_equal
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
    #define cmd_equal(cmd) jsmn_tokeq (mi->msg, &tok[method], cmd)
    if ((method = parse_message (mi->msg, tok, &ntok)) == -1)
        goto cleanup;
    else if (cmd_equal ("connect"))
        process_connect (mi, tok, ntok);
    else if (cmd_equal ("disconnect"))
        process_disconnect (mi, tok, ntok);
    #undef cmd_equal

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

static _Noreturn void usage (void)
{
    puts ("usage: infradir [nclients] [port]\n"
          "\twhere 'nclients' is the maximum number of clients\n"
          "\tand 'port' is the port number to listen on");
    exit (EXIT_FAILURE);
}

void *udp_serve (void *data)
{
    int sockfd;

    pthread_detach (pthread_self ());

    sockfd = udp_server_init (((struct settings*)data)->port);
    udp_server_main (sockfd, ((struct settings*)data)->max_threads,
            handle_message);
}

static int ini_handler (void *user, const char *section, const char *name,
        const char *value)
{
    int val;

    if (strcmp (section, "Directory"))
        return 1;

    if (!strcmp (name, "listen-port")) {
        if (!(val = atoi (value)))
            printf ("%s: error: listen-port must be a positive integer\n",
                    (char*) user);
        else
            settings.port = strdup (value);
    } else if (!strcmp (name, "max-threads")) {
        if (!(val = atoi (value)))
            printf ("%s: error: max-threads must be a positive integer\n",
                    (char*) user);
        else
            settings.max_threads = val;
    }
    return 1;
}

void parse_opts (int argc, char *argv[], struct settings *dst)
{
    int c;
    char *endptr;

    for(;;) {
        static struct option long_options[] = {
            { "max-threads", required_argument, 0, 't' },
            { "listen-port", required_argument, 0, 'l' },
            { 0, 0, 0, 0 }
        };

        int options_index = 0;

        c = getopt_long (argc, argv, "t:l:", long_options, &options_index);

        if (c == -1)
            break;

        switch (c) {
        case 't':
            endptr = NULL;
            dst->max_threads = (int) strtol (optarg, &endptr, 10);
            if (dst->max_threads < 1 || (endptr && *endptr != '\0')) {
                puts ("error: --threads argument must be a positive integer");
                usage ();
            }
            break;

        case 'l':
            endptr = NULL;
            dst->port = optarg;
            if (strtol (optarg, &endptr, 10) < 1
                    || (endptr && *endptr != '\0')) {
                puts ("error: --listen-port argument "
                        "must be a positive integer");
                usage ();
            }
            break;

        case '?':
            break;

        default:
            usage ();
        }
    }
}

/*-----------------------------------------------------------------------------
 * Main... */
//-----------------------------------------------------------------------------
int main (int argc, char *argv[])
{
    int sockfd;
    pthread_t tid;

    if (ini_parse (RC_FILE, ini_handler, RC_FILE))
        printf ("Warning: failed to parse %s\n", RC_FILE);
    parse_opts (argc, argv, &settings);

#ifdef DAEMON
    daemonize ();
#endif

    num_threads = 0;
    pthread_mutex_init (&num_threads_lock, NULL);

    clients_init ();

    if (pthread_create (&tid, NULL, udp_serve, &settings))
        perror ("pthread_create");

    sockfd = tcp_server_init (settings.port);
    tcp_server_main (sockfd, settings.max_threads, handle_connection);
}
