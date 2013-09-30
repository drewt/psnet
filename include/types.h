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

#ifndef _TYPES_H
#define _TYPES_H

#include <sys/socket.h>
#include <netinet/in.h> /* INET6_ADDRSTRLEN */
#include <arpa/inet.h>

#include "ipv6.h"
#include "list.h"

#define MSG_MAX 512

#define PORT_MIN 0
#define PORT_MAX 665535
#define PORT_STRLEN 5

/*
 * For now, a PSNET handle is just a pointer to a struct that wraps around a
 * sockaddr structure.
 */
struct __psnet_entity {
	struct sockaddr_storage addr;
};
typedef struct __psnet_entity PSNET;

/*
 * An entry in a linked list of sockaddr structures.  This is what is returned
 * by JSON list parsing functions.
 */
struct psnet_list_entry {
	struct list_head chain;
	struct sockaddr_storage addr;
};

/* 
 * An entry in a linked list of strings.  This is used when generating JSON
 * to be sent over the network.
 */
struct response_node {
	struct list_head chain;
	char *data;
	size_t len;
};

/*
 * A UDP or TCP message from a client.
 */
struct msg_info {
    int sock;
    int socktype;
    size_t len;
    struct sockaddr_storage addr;
    char msg[MSG_MAX];
    char paddr[INET6_ADDRSTRLEN];
};

/*
 * Creates a PSNET handle from the given host and port.  Be warned: this
 * function does DNS lookup.
 */
PSNET *psnet_new(const char *host, const char *port);

/* 
 * Frees a PSNET handle.
 */
void psnet_free(PSNET *p);

static inline struct sockaddr *psnet_list_entry_addr(struct list_head *entry)
{
	return (struct sockaddr*) &((struct psnet_list_entry*)entry)->addr;
}

/*
 * Writes the (human-readable) IP address associated with the given PSNET
 * handle into the buffer dst.
 */
static inline int psnet_ntop(PSNET *ent, char *dst)
{
	const char *ret;
	struct sockaddr *addr = (struct sockaddr*) &ent->addr;
	ret = inet_ntop(ent->addr.ss_family, get_in_addr(addr), dst,
			get_addr_strlen(addr));
	return ret == NULL ? -1 : 0;
}

/*
 * Returns the port number associated with the given PSNET handle in host
 * byte-order.
 */
static inline uint16_t psnet_get_port(PSNET *ent)
{
	return ntohs(get_in_port((struct sockaddr*)&ent->addr));
}

#endif
