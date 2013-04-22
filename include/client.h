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

#ifndef _PSNET_CLIENT_H_
#define _PSNET_CLIENT_H_

#include <sys/socket.h>

struct msg_info;

enum client_rc {
    CL_OK,
    CL_BADIP,
    CL_BADPORT,
    CL_BADNUM,
    CL_NOTFOUND
};

struct response_node;

void clients_init (void);
int add_client (struct sockaddr_storage *addr, const char *port);
int remove_client (struct sockaddr_storage *addr, const char *port);
int clients_to_json (struct response_node **dest, struct sockaddr_storage *ign,
        const char *n);
int flood_to_clients (struct msg_info *mi);
unsigned int client_list_size (void);

#endif
