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
