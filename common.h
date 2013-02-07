/* common.h : common header
 *
 * Author: Drew Thoreson
 */

#include <pthread.h>

void *handle_request (void *data);

extern int num_threads;
extern pthread_mutex_t num_threads_lock;
