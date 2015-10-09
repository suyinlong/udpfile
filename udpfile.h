#ifndef __udpfile_h
#define __udpfile_h

#include "unp.h"
#include "unpthread.h"
#include "unpifiplus.h"

#define SOCK_NOTINIT   -1

struct socket_info {
    int sockfd;
    struct sockaddr     *addr;      /* primary address */
    struct sockaddr     *ntmaddr;   /* netmask address */
    struct sockaddr     *subnaddr;  /* subnet address */
    struct socket_info  *next;      /* next of these structures */
};

// Buffer size definition
#define IP_BUFFSIZE         20
#define FILENAME_BUFFSIZE   255

// function headers
extern struct ifi_info *Get_ifi_info_plus(int family, int doaliases);
extern        void      free_ifi_info_plus(struct ifi_info *ifihead);


#endif
