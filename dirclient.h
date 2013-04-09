#ifndef _P2P_DIRCLIENT_H_
#define _P2P_DIRCLIENT_H_

#include "common.h"

enum dir_status { STATUS_OKAY = 0, STATUS_ERROR = -1 };

int dir_discover (struct node_list *prev, char *host, char *host_port,
        char *listen_port, int n);
int dir_list (struct node_list *prev, char *host, char *port, int n);
int dir_connect (char *host, char *host_port, char *listen_port, int *status);

#endif
