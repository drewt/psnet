#ifndef _P2P_CLIENT_H_
#define _P2P_CLIENT_H_

#include "response.h"

enum client_rc {
    CL_OK,
    CL_BADIP,
    CL_BADPORT,
    CL_BADNUM,
    CL_NOTFOUND
};

int add_client (struct sockaddr_storage *addr, const char *port);
int remove_client (struct sockaddr_storage *addr, const char *port);
int clients_to_json (struct response_node **dest, const char *n);

#endif
