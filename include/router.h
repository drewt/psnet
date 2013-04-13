#ifndef _PSNET_ROUTER_H_
#define _PSNET_ROUTER_H_

#ifndef DIR_RETRY_INTERVAL
#define DIR_RETRY_INTERVAL 30
#endif

#ifndef ROUTERS_UPDATE_INTERVAL
#define ROUTERS_UPDATE_INTERVAL 30
#endif

#ifndef DIR_KEEPALIVE_INTERVAL
#define DIR_KEEPALIVE_INTERVAL 9
#endif

struct msg_info;
struct response_node;

void router_init (char *dir_addr, char *dir_port, char *listen_port);
void flood_message (struct msg_info *mi);
void routers_to_json (struct response_node **dst, int n);

#endif
