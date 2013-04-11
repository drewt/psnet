#ifndef _PSNET_RESPONSE_H_
#define _PSNET_RESPONSE_H_

#include <stdbool.h>

/* node in linked list of response strings */
struct response_node {
    char *data;
    size_t size;
    struct response_node *next;
};

int send_response (int sock, struct response_node *node);
void make_simple_response (struct response_node *prev, const char *data,
        size_t data_size);
void make_response_with_body (struct response_node *head,
        struct response_node *body);
void free_response (struct response_node *node);

static inline void response_bad (struct response_node *prev) {
#ifdef LISP_OUTPUT
    make_simple_response (prev, "(:status \"error\")\r\n\r\n", 21);
#else
    make_simple_response (prev, "{\"status\":\"error\"}\r\n\r\n", 22);
#endif
}

static inline void response_ok (struct response_node *prev) {
#ifdef LISP_OUTPUT
    make_simple_response (prev, "(:status \"okay\")\r\n\r\n", 20);
#else
    make_simple_response (prev, "{\"status\":\"okay\"}\r\n\r\n", 21);
#endif
}

static inline bool cmd_equal (const char *str, const char *cmd, size_t len) {
    return !strncmp (str, cmd, len) && str[len] == '\0';
}

#endif
