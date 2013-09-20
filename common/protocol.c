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
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>

#include "ipv6.h"
#include "network.h"
#include "parse.h"
#include "protocol.h"

#define HDR_MAX 512

ssize_t psnet_request(PSNET *ent, char **resp, size_t len, const char *msg)
{
	int sock;
	int r_status;
	size_t r_size;
	ssize_t rv;
	char hdr[HDR_MAX];
	struct sockaddr *addr = (struct sockaddr*) &ent->addr;

	if ((sock = socket(addr->sa_family, SOCK_STREAM, IPPROTO_TCP)) == -1)
		return -errno;

	if (connect(sock, addr, get_sockaddr_size(addr)) == -1)
		rv = -errno;
		goto cleanup;

	if ((rv = tcp_send_bytes(sock, msg, len)) < 0)
		goto cleanup;

	if ((rv = tcp_read_msg(sock, hdr, HDR_MAX)) <= 0)
		goto cleanup;

	if (rv == HDR_MAX) {
		rv = -EBADMSG;
        	goto cleanup;
	}

	if ((rv = parse_header(&r_status, &r_size, hdr)) == -1) {
		rv = -EBADMSG;
		goto cleanup;
	}

	if (r_size == 0 || resp == NULL) {
		rv = 0;
		goto cleanup;
	}

	*resp = malloc(r_size + 1);
	if ((rv = tcp_read_bytes(sock, *resp, r_size)) == -1) {
		free (*resp);
		goto cleanup;
	}
	(*resp)[rv] = '\0';

cleanup:
	close(sock);
	return rv;
}

ssize_t psnet_requestf(PSNET *ent, char **resp, size_t size, const char *fmt,
		...)
{
	int len;
	char msg[size];
	va_list ap;

	va_start(ap, fmt);
	len = vsnprintf(msg, size, fmt, ap);
	va_end(ap);

	return psnet_request(ent, resp, len, msg);
}

int psnet_send_connect(PSNET *ent, in_port_t port)
{
	return udp_sendf((struct sockaddr*)&ent->addr, 32 + 5,
			"{\"method\":\"connect\",\"port\":%d}", port);
}

int psnet_send_disconnect(PSNET *ent, in_port_t port)
{
	return udp_sendf((struct sockaddr*)&ent->addr, 35 + 5,
			"{\"method\":\"disconnect\",\"port\":%d}", port);
}

int psnet_raw_request_discover(PSNET *ent, char **dst, int num, int port)
{
	return psnet_requestf(ent, dst, 40 + 40 + 5, 
			"{\"method\":\"discover\",\"num\":%d,\"port\":%d}\r\n\r\n",
			num, port);
}

int psnet_request_discover(PSNET *ent, struct list_head *head, int num, int port)
{
	int rv;
	char *dst;

	rv = psnet_raw_request_discover(ent, &dst, num, port);
	if (rv < 0)
		return rv;

	rv = parse_node_list(head, dst, num);
	if (rv < 0)
		free(dst);
	return rv;
}

int psnet_request_info(PSNET *ent, char **dst)
{
	return psnet_request(ent, dst, 21, "{\"method\":\"info\"}\r\n\r\n");
}

int psnet_raw_request_list(PSNET *ent, char **dst, int num)
{
	return psnet_requestf(ent, dst, 28 + 5,
			"{\"method\":\"list\",\"num\":%d}\r\n\r\n", num);
}

int psnet_request_ping(PSNET *ent)
{
	return psnet_request(ent, NULL, 21, "{\"method\":\"ping\"}\r\n\r\n");
}

int psnet_send_response(int sock, struct list_head *head)
{
	struct list_head *pos;
	ssize_t rv;

	list_for_each(pos, head) {
		struct response_node *node = (struct response_node*) pos;
		rv = tcp_send_bytes(sock, node->data, node->len);
		if (rv < 0)
			return rv;
	}
	return 0;
}

void psnet_send_error(int sock, int no, const char *str)
{
	int len;
	char s[42 + 3 + PSNET_ERRSTRLEN];

	len = sprintf(s, "{\"status\":\"error\",\"code\":%d,"
			"\"reason\":\"%s\"}\r\n\r\n", no, str);
	tcp_send_bytes(sock, s, len);
}

void psnet_send_ok(int sock)
{
	tcp_send_bytes(sock, "{\"status\":\"okay\"}\r\n\r\n", 21);
}

void make_response_with_body(struct list_head *head, struct list_head *body)
{
	struct response_node *hdr;
	struct list_head *pos;
	size_t len = 0;

	list_for_each(pos, body) {
		len += ((struct response_node*)pos)->len;
	}

	hdr = malloc(sizeof(struct response_node));
	hdr->data = malloc(100);
	hdr->len = snprintf(hdr->data, 100,
			"{\"status\":\"okay\",\"size\":%lu}\r\n\r\n", len);

	list_add_tail((struct list_head*)hdr, head);
	list_splice_tail(body, head);
}

void free_response(struct list_head *head)
{
	struct response_node *it, *next;

	it = (struct response_node*) head->next;

	while ((struct list_head*)it != head) {
		next = (struct response_node*) it->chain.next;
		free(it->data);
		free(it);
		it = next;
	}
}

PSNET *psnet_new(const char *host, const char *port)
{
	PSNET *ent;
	struct addrinfo hints;
	struct addrinfo *servinfo;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;

	if (getaddrinfo(host, port, &hints, &servinfo) != 0)
		return NULL;

	ent = malloc(sizeof(PSNET));
	ent->addr = *((struct sockaddr_storage*)servinfo->ai_addr);

	freeaddrinfo(servinfo);
	return ent;
}

void psnet_free(PSNET *p)
{
	free(p);
}
