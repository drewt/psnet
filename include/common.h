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

#ifdef DAEMON
void daemonize (void);
#endif

static inline void *get_in_addr (const struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in*)sa)->sin_addr);
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

static inline in_port_t get_in_port (const struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
        return ((struct sockaddr_in*)sa)->sin_port;
    return ((struct sockaddr_in6*)sa)->sin6_port;
}

static inline void set_in_port (struct sockaddr *sa, in_port_t val)
{
    if (sa->sa_family == AF_INET)
        ((struct sockaddr_in*)sa)->sin_port = val;
    else
        ((struct sockaddr_in6*)sa)->sin6_port = val;
}

static inline int sin_equals (const struct sockaddr_in *a,
        const struct sockaddr_in *b)
{
    return a->sin_addr.s_addr == b->sin_addr.s_addr &&
        a->sin_port == b->sin_port;
}

static inline int sin6_equals (const struct sockaddr_in6 *a,
        const struct sockaddr_in6 *b)
{
    return !memcmp (a->sin6_addr.s6_addr, b->sin6_addr.s6_addr, 16) &&
        a->sin6_port == b->sin6_port;
}

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

#endif
