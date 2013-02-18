#ifndef _CLIENT_H_
#define _CLIENT_H_

enum client_rc {
    CL_OK,
    CL_BADIP,
    CL_BADPORT,
    CL_NOTFOUND
};

int add_client (const char *ip, const char *port);
int remove_client (const char *ip, const char *name);
char *clients_to_json (void);

#endif
