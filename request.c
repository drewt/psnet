#include <sys/socket.h>
#include "request.h"

#define BUF_SIZE 100

/* receive buffer (used only by recv_char) */
struct recv_buf {
    char data[BUF_SIZE];
    ssize_t pos;
    ssize_t len;
};

/* the message delimiter sequence */
static const struct {
    const char *str;
    size_t len;
} delim = { "\r\n\r\n", 4 };

/*-----------------------------------------------------------------------------
 * Reads a byte from the buffer, receiving more bytes from the socket if the
 * buffer is empty */
//-----------------------------------------------------------------------------
static signed char recv_char (int sock, struct recv_buf *buf) {

    if (buf->pos == buf->len) {
        buf->pos = 0;
        if ((buf->len = recv (sock, buf->data, BUF_SIZE, 0)) == -1)
            return -1;
        if (!buf->len)
            return -2;
    }
    buf->pos++;
    return buf->data[buf->pos - 1];
}

/*-----------------------------------------------------------------------------
 * Reads into the buffer (buf) until either the delimiter sequence is found or
 * the maximum message size (MSG_MAX) is reached.  Returns the message size on
 * a successful read, or 0 if the client closed the connection */
//-----------------------------------------------------------------------------
size_t read_request (int sock, char *msg_buf) {
    size_t i, delim_pos;
    struct recv_buf recv_buf = { .pos = 0, .len = 0 };

    for (i = 0, delim_pos = 0; i < REQ_MAX-1 && delim_pos < delim.len; i++) {

        signed char c = recv_char (sock, &recv_buf);
        if (c < 0)
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
