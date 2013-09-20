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

#ifndef _PROTOCOL_H
#define _PROTOCOL_H

#include <unistd.h> /* ssize_t */
#include <sys/socket.h>
#include <netinet/in.h>

#include "types.h"

#define PSNET_ERRSTRLEN 100

int psnet_raw_request_discover(PSNET *ent, char **dst, int num, int port);

int psnet_raw_request_list(PSNET *ent, char **dst, int num);

ssize_t psnet_request(PSNET *ent, char **resp, size_t len, const char *msg);

int psnet_request_discover(PSNET *ent, struct list_head *head, int num, int port);

int psnet_request_info(PSNET *ent, char **dst);

int psnet_request_list(PSNET *ent, struct list_head *head, int num);

int psnet_request_ping(PSNET *ent);

int psnet_send_connect(PSNET *ent, in_port_t port);

int psnet_send_disconnect(PSNET *ent, in_port_t port);

int psnet_send_response(int sock, struct list_head *head);

void psnet_send_error(int sock, int no, const char *str);

void psnet_send_ok(int sock);

void make_response_with_body(struct list_head *head, struct list_head *body);

void free_response(struct list_head *head);

#endif
