#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

void udp_send_msg (const char *msg, size_t msg_len)
{
    struct addrinfo hints, *servinfo;
    int sockfd;
    int rc;

    if ((sockfd = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        perror ("socket");
        exit (EXIT_FAILURE);
    }

    memset (&hints, 0, sizeof (hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags    = AI_PASSIVE;

    if ((rc = getaddrinfo ("127.0.0.1", "5555", &hints, &servinfo))) {
        fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (rc));
        exit (EXIT_FAILURE);
    }

    freeaddrinfo (servinfo);

    if (sendto (sockfd, msg, msg_len, 0, servinfo->ai_addr, servinfo->ai_addrlen) == -1)
        perror ("sendto");
}

void udp_send_msg0 (const char *msg, size_t len)
{
    struct in_addr addr;
    if (!inet_aton ("127.0.0.1", &addr)) {
        perror ("inet_aton");
        exit (EXIT_FAILURE);
    }

    struct sockaddr_in si;
    memset (&si, 0, sizeof si);
    si = (struct sockaddr_in) {
        .sin_family = AF_INET,
        .sin_port = htons (5555),
        .sin_addr = addr
    };

    int s;
    if ((s = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        perror ("socket");
        exit (EXIT_FAILURE);
    }

    if (sendto (s, msg, len, 0, (struct sockaddr*)&si, sizeof si) == -1) {
        perror ("sendto");
        exit (EXIT_FAILURE);
    }
}

int main (void)
{
    /*struct in_addr addr;
    inet_aton ("127.0.0.1", &addr);
    struct sockaddr_in si = {
        .sin_family = AF_INET,
        .sin_addr = addr,
        .sin_port = 5555
    };*/
    //udp_send_msg ("{\"method\":\"ping\",\"port\":5555}", 29);
    udp_send_msg0 ("{\"method\":\"ping\",\"port\":5555}", 29);
    return 0;
}
