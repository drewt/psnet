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
#include <string.h>
#include <arpa/inet.h>

#include "common.h"
#include "nodelist.h"
#include "jsmn.h"

/*-----------------------------------------------------------------------------
 * Parses a node object given by the token 'node' (pointing into the message
 * 'msg') into prev->next */
//-----------------------------------------------------------------------------
static int parse_node (struct node_list *prev, const char *msg, jsmntok_t *node)
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

    set_in_port ((struct sockaddr*) &ss, htons ((in_port_t) lport));

    prev->next = malloc (sizeof (struct node_list));
    prev->next->addr = ss;
#ifdef PSNETLOG
    strncpy (prev->next->paddr, msg + node[iip].start,
            jsmn_toklen (&node[iip]));
    prev->next->paddr[jsmn_toklen (&node[iip])] = '\0';
#endif

    return 0;
}

/*-----------------------------------------------------------------------------
 * Parses a node array of 'nentries' elements (given in 'msg') into
 * prev->next */
//-----------------------------------------------------------------------------
int parse_node_list (struct node_list *prev, char *msg, int nentries)
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
