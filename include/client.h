#ifndef _P2P_CLIENT_H_
#define _P2P_CLIENT_H_

#include <sys/socket.h>

enum client_rc {
    CL_OK,
    CL_BADIP,
    CL_BADPORT,
    CL_BADNUM,
    CL_NOTFOUND
};

struct response_node;

int add_client (struct sockaddr_storage *addr, const char *port);
int remove_client (struct sockaddr_storage *addr, const char *port);
int clients_to_json (struct response_node **dest, struct sockaddr_storage *ign,
        const char *n);

#endif
