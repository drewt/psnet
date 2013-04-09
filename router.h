#ifndef _P2P_ROUTER_H_
#define _P2P_ROUTER_H_

#include <sys/socket.h>

#ifndef DIR_RETRY_INTERVAL
#define DIR_RETRY_INTERVAL 30
#endif

#ifndef ROUTERS_UPDATE_INTERVAL
#define ROUTERS_UPDATE_INTERVAL 30
#endif

#ifndef DIR_KEEPALIVE_INTERVAL
#define DIR_KEEPALIVE_INTERVAL 9
#endif

void router_init (void);
void flood_message (struct msg_info *mi);

#endif
