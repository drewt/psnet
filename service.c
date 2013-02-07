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
enum req_id { LOGIN, LOGOUT, LIST, INVALID, CLOSED };
enum state  { STATE_LOGIN, STATE_LOGOUT, STATE_LIST, STATE_INIT };

struct cmd {
    const char *str; // string
    size_t     tp;   // termination point (i.e. when to stop reading)
};

/* a 'smart' data structure to simplify the code later.
 * commands are indexed by the corresponding state enum */
struct cmd cmds[4] = {
    [STATE_LOGIN]  = { "in",   2 },
    [STATE_LOGOUT] = { "out",  3 },
    [STATE_LIST]   = { "list", 4 },
    [STATE_INIT]   = { "",     1 } // 1 to accomodate initial state
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

        // ETX
        if (c == 3)
            return CLOSED;

        // initial state
        if (state == STATE_INIT) {
            if (c == cmds[STATE_LOGIN].str[0])
                state = STATE_LOGIN;
            else if (c == cmds[STATE_LOGOUT].str[0])
                state = STATE_LOGOUT;
            else if (c == cmds[STATE_LIST].str[0])
                state = STATE_LIST;
            else
                return INVALID;
        }

        // all command recognition states are essentially the same:
        // either transition to the 'next' state (i++) or the invalid state
        if (c != cmds[state].str[i])
            return INVALID;
    }

    // map state to request
    switch (state) {
    case STATE_LOGIN:
        return LOGIN;
    case STATE_LOGOUT:
        return LOGOUT;
    case STATE_LIST:
        return LIST;
    default:
        return INVALID;
    }
    return INVALID;
}

/*-----------------------------------------------------------------------------
 * Handles requests from the client */
//-----------------------------------------------------------------------------
void *handle_request (void *data) {

    ssize_t off, rc;
    uint32_t resp, enc;
    int sock;
    bool done;

    sock = (int) data;
    done = false;

    while (!done) {
        switch (get_request (sock)) {
            case LOGIN:
                resp = 1;
                break;
            case LOGOUT:
                resp = 2;
                break;
            case LIST:
                resp = 3;
                break;
            case CLOSED:
                goto cleanup;
            case INVALID:
                resp = -1;
                break;
        }

        // send response
        off = 0;
        enc = htonl (resp);
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
