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

#ifndef _PSNET_ROUTER_H_
#define _PSNET_ROUTER_H_

#ifndef DIR_RETRY_INTERVAL
#define DIR_RETRY_INTERVAL 30
#endif

#ifndef ROUTERS_UPDATE_INTERVAL
#define ROUTERS_UPDATE_INTERVAL 30
#endif

#ifndef DIR_KEEPALIVE_INTERVAL
#define DIR_KEEPALIVE_INTERVAL 9
#endif

struct msg_info;
struct response_node;

int router_init(char *dir_addr, char *dir_port, char *listen_port);
void flood_message(struct msg_info *mi);
void routers_to_json(struct list_head *head, int n);

#endif
