/* service.c : handles connections from clients
 *
 * Author: Drew Thoreson
 */

#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <time.h>

#include "common.h"

/* use separate enums for request id and command recognition state; this
 * makes it easier to extend the FSM */
enum req_id { UPTIME, LOAD, EXIT, INVALID, CLOSED };
enum state  { STATE_UPTIME = 0, STATE_LOAD = 1, STATE_EXIT = 2,
              STATE_INIT = 3 };

struct cmd {
    const char *str; // string
    size_t     tp;   // termination point (i.e. when to stop reading)
};

/* a 'smart' data structure to simplify the code later.
 * commands are indexed by the corresponding req_id */
struct cmd cmds[4] = {
    { "uptime", 6 },
    { "load",   4 },
    { "exit",   4 },
    { "",       1 }  // 1 to accomodate for initial state
};

/*-----------------------------------------------------------------------------
 * Receives bytes from the given socket one at a time until either a valid
 * command is recognized, or an invalid sequence of bytes is detected */
//-----------------------------------------------------------------------------
static enum req_id get_request (int sock) {
    char c;
    size_t i;
    ssize_t rc;
    enum state state = STATE_INIT;

    for (i = 0;i < cmds[state].tp; i++) {
        if ((rc = recv (sock, &c, 1, 0)) == -1) {
            perror ("recv");
            return CLOSED;
        } else if (!rc) {
            return CLOSED;
        }

        if (c == 3)
            return CLOSED;

        // initial state
        if (state == STATE_INIT) {
            if (c == cmds[STATE_UPTIME].str[0])
                state = UPTIME;
            else if (c == cmds[STATE_LOAD].str[0])
                state = LOAD;
            else if (c == cmds[STATE_EXIT].str[0])
                state = EXIT;
            else
                return INVALID;
        }

        // all command recognition states are essentially the same:
        // either transistion to the 'next' state (i++) or the invalid state
        if (c != cmds[state].str[i])
            return INVALID;
    }

    // map state to request
    switch (state) {
    case STATE_UPTIME:
        return UPTIME;
    case STATE_LOAD:
        return LOAD;
    case STATE_EXIT:
        return EXIT;
    case STATE_INIT:
        return INVALID;
    }
    return INVALID;;
}

/*-----------------------------------------------------------------------------
 * Handles requests from the client */
//-----------------------------------------------------------------------------
void *handle_request (void *data) {

    ssize_t off, rc;
    uint32_t resp, enc;
    int sock, bad_cmds = 0;
    time_t t;
    bool done;

    sock = (int) data;
    done = false;

    while (!done) {
        switch (get_request (sock)) {
            case UPTIME:
                bad_cmds = 0;
                if ((t = time (NULL)) == -1) {
                    perror ("time");
                    resp = -2;
                } else {
                    resp = t;
                }
                break;
            case LOAD:
                bad_cmds = 0;
                pthread_mutex_lock (&num_threads_lock);
                resp = num_threads;
                pthread_mutex_unlock (&num_threads_lock);
                break;
            case EXIT:
                resp = 0;
                done = true;
                break;
            case CLOSED:
                goto cleanup;
            case INVALID:
                puts ("got invalid command"); fflush (stdout);
                if (++bad_cmds > 2)
                    done = true;
                resp = -1;
                break;
        }

        // send response
        off = 0;
        //enc = htonl (resp);
        enc = resp;
        while ((rc = send (sock, (char*) &enc + off, 4 - off, 0)) != 4 - off)
            off += rc;
    }

cleanup:
    puts ("closing connection to client"); fflush (stdout);
    close (sock);
    pthread_mutex_lock (&num_threads_lock);
    num_threads--;
    pthread_mutex_unlock (&num_threads_lock);
    pthread_exit (NULL);
}
