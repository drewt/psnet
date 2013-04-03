/* common.h : common header
 *
 * Author: Drew Thoreson
 */

#include <netinet/in.h>
#include <pthread.h>

#ifdef DAEMON
#define ANSI_YELLOW ""
#define ANSI_GREEN  ""
#define ANSI_RED    ""
#define ANSI_RESET  ""
#else
#define ANSI_YELLOW "\x1b[33m"
#define ANSI_GREEN  "\x1b[32m"
#define ANSI_RED    "\x1b[31m"
#define ANSI_RESET  "\x1b[0m"
#endif

struct conn_info {
    int sock;
    char addr[INET6_ADDRSTRLEN];
};

/* node in linked list of response strings */
struct response_node {
    char *data;
    size_t size;
    struct response_node *next;
};

void *handle_request (void *data);

extern int num_threads;
extern pthread_mutex_t num_threads_lock;
