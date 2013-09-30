/* Copyright 2013 Drew Thoreson */

/* This file is part of libpsnet
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * libpsnet is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * libpsnet.  If not, see <http://www.gnu.org/licenses/>
 */

#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "jsmn.h"

#include "parse.h"
#include "ipv6.h"

static int parse_node(struct list_head *head, const char *msg, jsmntok_t *node)
{
	struct psnet_list_entry *new;
	char s[INET6_ADDRSTRLEN];
	int iip, iport, iipv;
	int ipv;
	int rc;
	char *endptr;
	long lport;

	/* get indices */
	rc = jsmn_get_values(msg, node, "ip", &iip, "port", &iport, "ipv",
			&iipv, (void*) NULL);
	if (rc == -1)
		return -1;

	/* check types */
	if (node[iip].type != JSMN_STRING)
		return -1;
	if (node[iport].type != JSMN_PRIMITIVE)
		return -1;
	if (msg[node[iport].start] < '0' || msg[node[iport].start] > '9')
		return -1;
	if (node[iipv].type != JSMN_PRIMITIVE)
		return -1;
	ipv = msg[node[iipv].start];
	if (ipv != '4' && ipv != '6')
		return -1;

	/* convert and set address */
	strncpy(s, msg + node[iip].start, jsmn_toklen(&node[iip]));
	s[jsmn_toklen(&node[iip])] = '\0';

	new = malloc(sizeof(struct psnet_list_entry));
	memset(&new->addr, 0, sizeof(struct sockaddr_storage));
	new->addr.ss_family = ipv == '4' ? AF_INET : AF_INET6;
	if (!inet_pton(new->addr.ss_family, s,
				get_in_addr((struct sockaddr*)&new->addr)))
		goto err;

	/* convert and set port */
	strncpy(s, msg + node[iport].start, jsmn_toklen(&node[iport]));
	s[jsmn_toklen(&node[iport])] = '\0';

	lport = strtol(s, &endptr, 10);
	if (lport < PORT_MIN || lport > PORT_MAX || *endptr != '\0')
		goto err;

	set_in_port((struct sockaddr*)&new->addr, htons((in_port_t)lport));

	list_add_tail((struct list_head*)new, head);
	return 0;
err:
	free(new);
	return -1;
}

int parse_node_list(struct list_head *head, char *msg, int nentries)
{
	const int max_tok = 1 + 10 * nentries;
	jsmn_parser p;
	jsmntok_t tok[max_tok];
	int rc, i, ntok, end;

	jsmn_init(&p);
	rc = jsmn_parse(&p, msg, tok, max_tok);
	if (rc != JSMN_SUCCESS || tok[0].type != JSMN_ARRAY)
		return -1;

	i = 1;
	ntok = tok[0].size + 1;
	while (i < ntok) {
		if (tok[i].type != JSMN_OBJECT || tok[i].size < 6)
			return -1;
		if (parse_node(head, msg, &tok[i]))
			return -1;

		/* find next element */
		for (end = 1; end; i++, end--) {
			end += tok[i].size;
			ntok += tok[i].size;
		}
	}
	return 0;
}

int parse_header (int *status, size_t *size, char *msg)
{
	char s[5];
	char *endptr;
	jsmn_parser p;
	jsmntok_t tok[256];
	int istatus, isize;
	long lsize;
	int rc;

	jsmn_init(&p);
	rc = jsmn_parse(&p, msg, tok, 256);
	if (rc != JSMN_SUCCESS || tok[0].type != JSMN_OBJECT)
		return -1;

	istatus = jsmn_get_value(msg, tok, "status");
	if (istatus == -1)
		return -1;

	isize = jsmn_get_value(msg, tok, "size");
	if (isize == -1) {
		*size = 0;
	} else if (tok[isize].type != JSMN_PRIMITIVE) {
		return -1;
	} else if (msg[tok[isize].start] < '0' || msg[tok[isize].start] > '9') {
		return -1;
	} else {
		strncpy(s, msg + tok[isize].start, jsmn_toklen(&tok[isize]));
		s[jsmn_toklen(&tok[isize])] = '\0';

		lsize = strtol (msg + tok[isize].start, &endptr, 10);
		if (lsize < 0)
			return -1;
		*size = (size_t) lsize;
	}

	*status = !strncmp(msg + tok[istatus].start, "okay", 4) ?
		PSNET_OKAY : PSNET_ERROR;

	return 0;
}

int parse_message (const char *msg, jsmntok_t *tok, size_t *ntok)
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
