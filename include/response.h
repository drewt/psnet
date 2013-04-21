#ifndef _PSNET_RESPONSE_H_
#define _PSNET_RESPONSE_H_

#define PSNET_ERRSTRLEN 100

/* node in linked list of response strings */
struct response_node {
    char *data;
    size_t size;
    struct response_node *next;
};

int tcp_send_bytes (int sock, char *buf, size_t len);
int send_response (int sock, struct response_node *node);
void make_response_with_body (struct response_node *head,
        struct response_node *body);
void free_response (struct response_node *node);
void send_error (int sock, int no, const char *str);
void send_ok (int sock);

static inline int cmd_equal (const char *str, const char *cmd, size_t len) {
    return !strncmp (str, cmd, len) && str[len] == '\0';
}

#endif
