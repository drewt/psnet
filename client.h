#ifndef _CLIENT_H_
#define _CLIENT_H_

#include "response.h"

enum client_rc {
    CL_OK,
    CL_BADIP,
    CL_BADPORT,
    CL_BADNUM,
    CL_NOTFOUND
};

int add_client (const char *ip, const char *port);
int remove_client (const char *ip, const char *name);
int clients_to_json (struct response_node **dest, const char *n);

#endif
