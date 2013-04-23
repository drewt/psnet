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

#ifndef _PSNET_COMMON_H_
#define _PSNET_COMMON_H_

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

#ifdef DAEMON
#define ANSI_YELLOW ""
#define ANSI_GREEN  ""
#define ANSI_RED    ""
#define ANSI_RESET  ""
#else
#define ANSI_YELLOW "\x1b[33m"
#define ANSI_GREEN  "\x1b[32m"
#define ANSI_RED    "\x1b[31m"
#define ANSI_RESET  "\x1b[0m"
#endif

#define PORT_MIN 0
#define PORT_MAX 65535
#define PORT_STRLEN 5

#define JSMN_NTOK 256

#define MSG_MAX 512

typedef struct jsmntok jsmntok_t;

int parse_message (const char *msg, jsmntok_t *tok, size_t *ntok);
#ifdef DAEMON
void daemonize (void);
#endif

struct msg_info {
    int sock;
    int socktype;
    size_t len;
    struct sockaddr_storage addr;
    char msg[MSG_MAX];
#ifdef PSNETLOG
    char paddr[INET6_ADDRSTRLEN];
#endif
};

extern int max_threads;
extern int num_threads;
extern pthread_mutex_t num_threads_lock;

/* returns the IP address in a sockaddr_in or sockaddr_in6 structure */
static inline void *get_in_addr (const struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in*)sa)->sin_addr);
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/* returns the port number in a sockaddr_in or sockaddr_in6 structure */
static inline in_port_t get_in_port (const struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
        return ((struct sockaddr_in*)sa)->sin_port;
    return ((struct sockaddr_in6*)sa)->sin6_port;
}

/* sets the port number of a sockaddr_in or sockaddr_in6 structure */
static inline void set_in_port (struct sockaddr *sa, in_port_t val)
{
    if (sa->sa_family == AF_INET)
        ((struct sockaddr_in*)sa)->sin_port = val;
    else
        ((struct sockaddr_in6*)sa)->sin6_port = val;
}

/* compares the IP addresses and port numbers of two sockaddr_in structures */
static inline int sin_equals (const struct sockaddr_in *a,
        const struct sockaddr_in *b)
{
    return a->sin_addr.s_addr == b->sin_addr.s_addr &&
        a->sin_port == b->sin_port;
}

/* compares the IP addresses and port numbers of two sockaddr_in6 structures */
static inline int sin6_equals (const struct sockaddr_in6 *a,
        const struct sockaddr_in6 *b)
{
    return !memcmp (a->sin6_addr.s6_addr, b->sin6_addr.s6_addr, 16) &&
        a->sin6_port == b->sin6_port;
}

/* compares the IP addresses and port numbers of two sockaddr structures */
static inline int sockaddr_equals (const struct sockaddr *a,
        const struct sockaddr *b)
{
    if (a->sa_family != b->sa_family)
        return 0;
    if (a->sa_family == AF_INET)
        return sin_equals ((struct sockaddr_in*)a, (struct sockaddr_in*)b);
    else if (a->sa_family == AF_INET6)
        return sin6_equals ((struct sockaddr_in6*)a, (struct sockaddr_in6*)b);
    return 0;
}

/* compares the IP addresses--but not the ports--of two sockaddr structures */
static inline int ip_addr_equals (const struct sockaddr *a,
        const struct sockaddr *b)
{
    if (a->sa_family != b->sa_family)
        return 0;
    if (a->sa_family == AF_INET) {
        return ((struct sockaddr_in*)a)->sin_addr.s_addr
            != ((struct sockaddr_in*)b)->sin_addr.s_addr;
    } else {
        return !memcmp (((struct sockaddr_in6*)a)->sin6_addr.s6_addr,
                ((struct sockaddr_in6*)b)->sin6_addr.s6_addr, 16);
    }
}

#endif
