/* common.h : common header
 *
 * Author: Drew Thoreson
 */

#include <netinet/in.h>
#include <pthread.h>

struct conn_info {
    int sock;
    char addr[INET6_ADDRSTRLEN];
};

void *handle_request (void *data);

extern int num_threads;
extern pthread_mutex_t num_threads_lock;
