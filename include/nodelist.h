#ifndef _P2P_NODELIST_H_
#define _P2P_NODELIST_H_

#include "common.h"

/* linked list of sockaddr* structures */
struct node_list {
    struct sockaddr_storage addr;
    struct node_list *next;
};

int parse_node_list (struct node_list *prev, char *msg, int nentries);

#endif
