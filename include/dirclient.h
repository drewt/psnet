#ifndef _PSNET_DIRCLIENT_H_
#define _PSNET_DIRCLIENT_H_

struct node_list;

enum dir_status { STATUS_OKAY = 0, STATUS_ERROR = -1 };

int dir_discover (struct node_list *prev, char *host, char *host_port,
        char *listen_port, int n);
int dir_list (struct node_list *prev, char *host, char *port, int n);
int dir_connect (char *host, char *host_port, char *listen_port);

#endif
