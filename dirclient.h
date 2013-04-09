#ifndef _P2P_DIRCLIENT_H_
#define _P2P_DIRCLIENT_H_

#include "common.h"

enum dir_status { STATUS_OKAY = 0, STATUS_ERROR = -1 };

int dir_get_list (struct node_list *prev, char *host, char *port);
int dir_send_connect (char *host, char *port, int *status);

#endif
