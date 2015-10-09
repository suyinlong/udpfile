/*
* @Author: Yinlong Su
* @Date:   2015-10-08 21:51:32
* @Last Modified by:   Yinlong Su
* @Last Modified time: 2015-10-09 16:55:16
*
* File:         udpserver.c
* Description:  Server C file
*/

#include "udpfile.h"

int port = 0;
int max_winsize = 0;

/* --------------------------------------------------------------------------
 *  readArguments
 *
 *  Read arguments from file
 *
 *  @param  : void
 *  @return : void
 *  @see    : file#server.in, var#port, var#max_winsize
 *
 *  Read arguments from "server.in"
 *    Line 1: <INTEGER>     -> int port
 *    Line 2: <INTEGER>     -> int max_winsize
 * --------------------------------------------------------------------------
 */
void readArguments() {
    FILE *fp;
    fp = Fopen("server.in", "rt");
    fscanf(fp, "%d", &port);
    fscanf(fp, "%d", &max_winsize);
    printf("[server.in] port=%d, max_winsize=%d\n", port, max_winsize);
}

/* --------------------------------------------------------------------------
 *  bind_sockets
 *
 *  Bind all unicast IP address to sockets
 *
 *  @param  : struct socket_info **
 *  @return : int
 *  @see    : function#Get_ifi_info_plus, struct#socket_info
 *
 *  Use Get_ifi_info_plus() to get interfaces information
 *  Build socket_info structure for our own server
 *  Bind all unicast address to socket
 *  Return the max file descriptor for sockets
 * --------------------------------------------------------------------------
 */
int bind_sockets(struct socket_info **sock_list) {
    const int on = 1;
    int maxfd = 0, i, sockfd;
    struct ifi_info     *ifi, *ifihead;
    struct sockaddr_in  *sa;
    struct socket_info  *slisthead = NULL, *slistprev = NULL, *slist = NULL;

    for (ifihead = ifi = Get_ifi_info_plus(AF_INET, 1); ifi != NULL; ifi = ifi->ifi_next) {
        // initialize the socket_info item
        slist = Malloc(sizeof(struct socket_info));
        slist->sockfd = SOCK_NOTINIT;
        slist->addr = Malloc(sizeof(struct sockaddr));
        slist->ntmaddr = Malloc(sizeof(struct sockaddr));
        slist->subnaddr = Malloc(sizeof(struct sockaddr));
        slist->next = NULL;

        if (!slisthead)
            slisthead = slist;
        if (slistprev)
            slistprev->next = slist;

        // fill addr, ntmaddr and subnaddr
        memcpy(slist->addr, ifi->ifi_addr, sizeof(struct sockaddr));
        memcpy(slist->ntmaddr, ifi->ifi_ntmaddr, sizeof(struct sockaddr));
        for (i = 0; i < sizeof(struct sockaddr); i++)
            *(((char*)slist->subnaddr)+i) = *(((char*)slist->addr)+i) & *(((char*)slist->ntmaddr)+i);

        // bind unicast address
        sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
        Setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
        sa = (struct sockaddr_in *) slist->addr;
        sa->sin_family = AF_INET;
        sa->sin_port = htons(port);
        Bind(sockfd, (SA *)sa, sizeof(*sa));

        // fill socket fd and save the max one
        slist->sockfd = sockfd;
        slistprev = slist;
        maxfd = max(maxfd, sockfd);
    }

    // free space of ifi_info
    free_ifi_info_plus(ifihead);
    *sock_list = slisthead;
    return maxfd;
}

/* --------------------------------------------------------------------------
 *  main
 *
 *  Entry function
 *
 *  @param  : int   argc
 *            char  **argv
 *  @return : int
 *  @see    : function#readArguments, function#bind_sockets
 *
 *  Server entry function
 * --------------------------------------------------------------------------
 */
int main(int argc, char **argv) {
    int maxfdp1 = -1;
    struct socket_info *sock_head = NULL, *sock = NULL;

    readArguments();
    maxfdp1 = bind_sockets(&sock_head) + 1;

    // print out the binding information
    for (sock = sock_head; sock != NULL; sock = sock->next) {
        printf("[ sockfd=%d\n", sock->sockfd);
        printf("\tIP address:     %s\n", Sock_ntop_host(sock->addr, sizeof(*(sock->addr))));
        printf("\tNetwork mask:   %s\n", Sock_ntop_host(sock->ntmaddr, sizeof(*(sock->ntmaddr))));
        printf("\tSubnet address: %s\n", Sock_ntop_host(sock->subnaddr, sizeof(*(sock->subnaddr))));
        printf("]\n");
    }

    while (1) {
        sleep(1);
    }
    exit(0);
}
