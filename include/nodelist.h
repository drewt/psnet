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

#ifndef _PSNET_NODELIST_H_
#define _PSNET_NODELIST_H_

#include <sys/socket.h>
#ifdef PSNETLOG
#include <netinet/in.h>
#endif

/* linked list of sockaddr* structures */
struct node_list {
    struct sockaddr_storage addr;
    struct node_list *next;
#ifdef PSNETLOG
    char paddr[INET6_ADDRSTRLEN];
#endif
};

int parse_node_list (struct node_list *prev, char *msg, int nentries);

#endif
