#include <stdio.h>
#include <sys/socket.h>

#include "tcp.h"

/* the message delimiter sequence */
static const struct {
    const char *str;
    size_t len;
} delim = { "\r\n\r\n", 4 };

/*-----------------------------------------------------------------------------
 * Reads into the buffer (buf) until either the delimiter sequence is found or
 * the maximum message size (MSG_MAX) is reached.  Returns the message size on
 * a successful read, or 0 if the client closed the connection */
//-----------------------------------------------------------------------------
size_t tcp_read_message (int sock, char *msg_buf)
{
    size_t i, delim_pos;
    ssize_t rv;

    for (i = 0, delim_pos = 0; i < TCP_MSG_MAX-1 && delim_pos < delim.len; i++) {

        signed char c;
        if ((rv = recv (sock, &c, 1, 0)) == -1)
            return 0;
        if (!rv)
            return 0;

        if (c == delim.str[delim_pos])
            delim_pos++;
        else
            delim_pos = 0;

        msg_buf[i] = c;
    }

    msg_buf[i] = '\0';
    return i;
}

/*-----------------------------------------------------------------------------
 * Reads from the given socket info the buffer 'msg_buf' until 'bytes' bytes
 * have been read */
//-----------------------------------------------------------------------------
size_t tcp_read_bytes (int sock, char *msg_buf, size_t bytes)
{
    ssize_t rv;
    size_t bytes_read = 0;

    while (bytes_read < bytes) {
        if ((rv = recv (sock, msg_buf+bytes_read, bytes-bytes_read, 0)) == -1) {
            perror ("recv");
            return -1;
        } else if (rv == 0) {
            break;
        }
        bytes_read += rv;
    }
    return bytes_read;
}
