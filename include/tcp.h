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

#ifndef _PSNET_TCP_H_
#define _PSNET_TCP_H_

#include <sys/socket.h>
#include <pthread.h>
#ifdef PSNETLOG
#include <netinet/in.h>
#endif

int tcp_server_init (char *port);
_Noreturn void tcp_server_main (int sock, void*(*cb)(void*));

size_t tcp_read_message (int sock, char *msg_buf);
size_t tcp_read_bytes (int sock, char *msg_buf, size_t bytes);

#endif
