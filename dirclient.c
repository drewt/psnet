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
#include "tcp.h"
#include "jsmn.h"

/*-----------------------------------------------------------------------------
 * Parses a header from the directory, setting the values of 'status' and
 * 'size' to the values contained in the header */
//-----------------------------------------------------------------------------
int parse_header (int *status, size_t *size, char *msg)
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

/*-----------------------------------------------------------------------------
 * Parses a node object given by the token 'node' (pointing into the message
 * 'msg') into prev->next */
//-----------------------------------------------------------------------------
int parse_node (struct node_list *prev, const char *msg, jsmntok_t *node)
{
    char s[INET6_ADDRSTRLEN];
    struct sockaddr_storage ss;
    int iip, iport, iipv;
    int ipv;
    char *endptr;
    long lport;

    // get indices
    if ((iip = jsmn_get_value (msg, node, "ip")) == -1)
        return -1;

    if ((iport = jsmn_get_value (msg, node, "port")) == -1)
        return -1;

    if ((iipv = jsmn_get_value (msg, node, "ipv")) == -1)
        return -1;

    // check types
    if (node[iip].type != JSMN_STRING)
        return -1;

    if (node[iport].type != JSMN_PRIMITIVE)
        return -1;
    if (msg[node[iport].start] < '0' || msg[node[iport].start] > '9')
        return -1;

    if (node[iipv].type != JSMN_PRIMITIVE)
        return -1;
    if (msg[node[iipv].start] < '4' || msg[node[iipv].start] > '6')
        return -1;

    // convert and set address
    ipv = msg[node[iipv].start] - '0';
    if (ipv != 4 && ipv != 6)
        return -1;

    strncpy (s, msg + node[iip].start, jsmn_toklen (&node[iip]));
    s[jsmn_toklen (&node[iip])] = '\0';

    memset (&ss, 0, sizeof ss);
    ss.ss_family = ipv == 4 ? AF_INET : AF_INET6;
    if (!inet_pton (ss.ss_family, s, get_in_addr ((struct sockaddr*)&ss)))
        return -1;

    // convert and set port
    strncpy (s, msg + node[iport].start, jsmn_toklen (&node[iport]));
    s[jsmn_toklen (&node[iport])] = '\0';

    lport = strtol (s, &endptr, 10);
    if (lport < PORT_MIN || lport > PORT_MAX || *endptr != '\0')
        return -1;

    set_in_port ((struct sockaddr*) &ss, (in_port_t) lport);

    prev->next = malloc (sizeof (struct node_list));
    prev->next->addr = ss;

    return 0;
}

/*-----------------------------------------------------------------------------
 * Parses a node array of 'nentries' elements (given in 'msg') into
 * prev->next */
//-----------------------------------------------------------------------------
int parse_node_array (struct node_list *prev, char *msg, int nentries)
{
    const int max_tok = 1 + 10 * nentries; // multiplier of 7 should do...
    jsmn_parser p;
    jsmntok_t tok[max_tok];
    int rc = 0, i, ntok, end;

    // parse message
    jsmn_init (&p);
    rc = jsmn_parse (&p, msg, tok, max_tok);
    if (rc != JSMN_SUCCESS || tok[0].type != JSMN_ARRAY) {
        rc = -1;
        goto cleanup;
    }

    ntok = tok[0].size + 1;
    for (i = 1; i < ntok; prev = prev->next) {
        if (tok[i].type != JSMN_OBJECT || tok[i].size < 6) {
            rc = -1;
            goto cleanup;
        }
        if (parse_node (prev, msg, &tok[i])) {
            rc = -1;
            goto cleanup;
        }
        // find next element
        for (end = 1; end; i++, end--) {
            end += tok[i].size;
            ntok += tok[i].size;
        }
    }
cleanup:
    prev->next = NULL;
    return rc;
}

/*-----------------------------------------------------------------------------
 * Sends 'cmd' to the directory given by 'host' and 'port'.  If 'dest' is not
 * NULL, it will point to a string containing the response from the directory
 * (minus the header) when this function returns.  The memory allocated for
 * this data should be freed by the caller */
//-----------------------------------------------------------------------------
static int send_command (char **dest, const char *cmd, char *host, char *port,
        int *status)
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

    *dest = malloc (size);
    if ((rv = tcp_read_bytes (sockfd, *dest, size)) == -1)
        fprintf (stderr, "send_command: failed to read message body\n");
    else if ((size_t) rv < size)
        fprintf (stderr, "directory sent fewer than expected bytes\n");

cleanup:
    close (sockfd);
    return rv;
}

/*-----------------------------------------------------------------------------
 * Sends a LIST command to the directory given by 'host' and 'port', parses the
 * response, and puts it in prev->next */
//-----------------------------------------------------------------------------
int dir_get_list (struct node_list *prev, char *host, char *port)
{
    int status;
    char *list;
    char cmd[22];

    snprintf (cmd, 22, "LIST %d %d\r\n\r\n", 1220, 32);
    if (send_command (&list, cmd, host, port, &status) < 1) {
        fprintf (stderr, "get_list: failed to retrieve list from directory\n");
        return -1;
    }
    if (parse_node_array (prev, list, 32)) {
        fprintf (stderr, "invalid router list from directory server\n");
        free (list);
        return -1;
    }

    free (list);
    return 0;
}

/*-----------------------------------------------------------------------------
 * Sends a CONNECT command to the directory given by 'host' and 'port' */
//-----------------------------------------------------------------------------
int dir_send_connect (char *host, char *port, int *status)
{
    return send_command (NULL, "CONNECT 1220\r\n\r\n", host, port, status);
}
