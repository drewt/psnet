#ifndef _PSNET_CLIENT_H_
#define _PSNET_CLIENT_H_

#include <sys/socket.h>

struct msg_info;

enum client_rc {
    CL_OK,
    CL_BADIP,
    CL_BADPORT,
    CL_BADNUM,
    CL_NOTFOUND
};

struct response_node;

void clients_init (void);
int add_client (struct sockaddr_storage *addr, const char *port);
int remove_client (struct sockaddr_storage *addr, const char *port);
int clients_to_json (struct response_node **dest, struct sockaddr_storage *ign,
        const char *n);
int flood_to_clients (struct msg_info *mi);
unsigned int client_list_size (void);

#endif
