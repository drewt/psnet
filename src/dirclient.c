#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "dirclient.h"
#include "common.h"
#include "nodelist.h"
#include "tcp.h"
#include "udp.h"
#include "jsmn.h"

/*-----------------------------------------------------------------------------
 * Parses a header from the directory, setting the values of 'status' and
 * 'size' to the values contained in the header */
//-----------------------------------------------------------------------------
static int parse_header (int *status, size_t *size, char *msg)
{
    char s[5];
    char *endptr;
    jsmn_parser p;
    jsmntok_t tok[256];
    int istatus, isize;
    long lsize;
    int rc;

    jsmn_init (&p);
    rc = jsmn_parse (&p, msg, tok, 256);
    if (rc != JSMN_SUCCESS || tok[0].type != JSMN_OBJECT)
        return -1;

    if ((istatus = jsmn_get_value (msg, tok, "status")) == -1)
        return -1;

    if ((isize = jsmn_get_value (msg, tok, "size")) == -1) {
        *size = 0;
    } else if (tok[isize].type != JSMN_PRIMITIVE) {
        return -1;
    } else if (msg[tok[isize].start] < '0' || msg[tok[isize].start] > '9') {
        return -1;
    } else {
        strncpy (s, msg + tok[isize].start, jsmn_toklen (&tok[isize]));
        s[jsmn_toklen (&tok[isize])] = '\0';

        lsize = strtol (msg + tok[isize].start, &endptr, 10);
        if (lsize < 0)
            return -1;
        *size = (int) lsize;
    }

    if (!strncmp (msg + tok[istatus].start, "okay", 4))
        *status = STATUS_OKAY;
    else {
        printf ("status is: %.*s\n", jsmn_toklen (&tok[istatus]),
                msg + tok[istatus].start);
        *status = STATUS_ERROR;
    }

    return 0;
}

static int udp_send_command (const char *cmd, size_t len, const char *host,
        const char *port)
{
    struct addrinfo hints, *servinfo;
    int rv;

    memset (&hints, 0, sizeof (hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo (host, port, &hints, &servinfo)) != 0) {
        fprintf (stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    udp_send_msg (cmd, len, servinfo->ai_addr);

    freeaddrinfo (servinfo);

    return 0;
}

/*-----------------------------------------------------------------------------
 * Sends 'cmd' to the directory given by 'host' and 'port'.  If 'dest' is not
 * NULL, it will point to a string containing the response from the directory
 * (minus the header) when this function returns.  The memory allocated for
 * this data should be freed by the caller */
//-----------------------------------------------------------------------------
static int tcp_send_command (char **dest, const char *cmd, const char *host,
        const char *port, int *status)
{
    size_t size;
    int sockfd, numbytes;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char msg[TCP_MSG_MAX];

    memset (&hints, 0, sizeof (hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo (host, port, &hints, &servinfo)) != 0) {
        fprintf (stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket (p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            continue;
        }

        if (connect (sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close (sockfd);
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf (stderr, "client: failed to connect\n");
        return -1;
    }

    freeaddrinfo (servinfo);

    if ((numbytes = send (sockfd, cmd, strlen (cmd), 0)) == -1) {
        perror ("send");
        rv = -1;
        goto cleanup;
    }

    if (!(rv = tcp_read_message (sockfd, msg))) {
        rv = 0;
        goto cleanup;
    }

    if (parse_header (status, &size, msg)) {
        fprintf (stderr, "invalid header from directory server\n");
        rv = -1;
        goto cleanup;
    }

    if (*status != STATUS_OKAY) {
        fprintf (stderr, "directory server reported error\n");
        rv = -1;
        goto cleanup;
    }

    if (size <= 0) {
        rv = 0;
        goto cleanup;
    }

    if (!dest) {
        rv = 0;
        goto cleanup;
    }

    *dest = malloc (size+1);
    if ((rv = tcp_read_bytes (sockfd, *dest, size)) == -1)
        fprintf (stderr, "send_command: failed to read message body\n");
    else if ((size_t) rv < size)
        fprintf (stderr, "directory sent fewer than expected bytes\n");
    (*dest)[rv] = '\0';

cleanup:
    close (sockfd);
    return rv;
}

/*-----------------------------------------------------------------------------
 * Sends a DISCOVER command to the directory given by 'host' and 'port', parses
 * the response, and puts it in prev->next */
//-----------------------------------------------------------------------------
int dir_discover (struct node_list *prev, char *host, char *host_port,
        char *listen_port, int n)
{
#define DISCOVER_STRLEN (20 + PORT_STRLEN)
    int status;
    char *list;
    char cmd[DISCOVER_STRLEN];

    snprintf (cmd, DISCOVER_STRLEN, "DISCOVER %s %d\r\n\r\n", listen_port, n);
    if (tcp_send_command (&list, cmd, host, host_port, &status) < 1) {
        fprintf (stderr, "get_list: failed to retrieve list from directory\n");
        return -1;
    }
    if (parse_node_list (prev, list, 32)) {
        fprintf (stderr, "invalid router list from directory server\n");
        free (list);
        return -1;
    }

    free (list);
    return 0;
#undef DISCOVER_STRLEN
}

/*-----------------------------------------------------------------------------
 * Sends a LIST command to the directory given by 'host' and 'port', parses the
 * response, and puts it in prev->next */
//-----------------------------------------------------------------------------
int dir_list (struct node_list *prev, char *host, char *port, int n)
{
#define LIST_STRLEN 15
    int status;
    char *list;
    char cmd[LIST_STRLEN];

    snprintf (cmd, LIST_STRLEN, "LIST %d\r\n\r\n", n);
    if (tcp_send_command (&list, cmd, host, port, &status) < 1) {
        fprintf (stderr, "get_list: failed to retrieve list from directory\n");
        return -1;
    }
    if (parse_node_list (prev, list, 32)) {
        fprintf (stderr, "invalid router list from directory server\n");
        free (list);
        return -1;
    }

    free (list);
    return 0;
#undef LIST_STRLEN
}

/*-----------------------------------------------------------------------------
 * Sends a CONNECT command to the directory given by 'host' and 'port' */
//-----------------------------------------------------------------------------
int dir_connect (char *host, char *host_port, char *listen_port)
{
#define CONNECT_STRLEN (13 + PORT_STRLEN)
    char s[CONNECT_STRLEN];
    size_t len;
    len = snprintf (s, CONNECT_STRLEN, "CONNECT %s\r\n\r\n", listen_port);
    return udp_send_command (s, len, host, host_port);
#undef CONNECT_STRLEN
}
