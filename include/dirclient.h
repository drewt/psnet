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

#ifndef _PSNET_DIRCLIENT_H_
#define _PSNET_DIRCLIENT_H_

struct node_list;

enum dir_status { STATUS_OKAY = 0, STATUS_ERROR = -1 };

int dir_discover (struct node_list *prev, char *host, char *host_port,
        char *listen_port, int n);
int dir_list (struct node_list *prev, char *host, char *port, int n);
int dir_connect (char *host, char *host_port, char *listen_port);

#endif
