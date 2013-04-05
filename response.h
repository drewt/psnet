#ifndef _RESPONSE_H_
#define _RESPONSE_H_

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

#endif
