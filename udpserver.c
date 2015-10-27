/*
* @Author: Yinlong Su
* @Date:   2015-10-08 21:51:32
* @Last Modified by:   Yinlong Su
* @Last Modified time: 2015-10-26 16:44:09
*
* File:         udpserver.c
* Description:  Server C file
*/

#include "udpfile.h"

int port = 0;
int max_winsize = 0;
struct process_info *proc_head = NULL, *proc = NULL;

/* --------------------------------------------------------------------------
 *  readArguments
 *
 *  Read arguments from file
 *
 *  @param  : void
 *  @return : void
 *  @see    : file#server.in
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
    Fclose(fp);
}

/* --------------------------------------------------------------------------
 *  bind_sockets
 *
 *  Bind all unicast IP address to sockets
 *
 *  @param  : struct socket_info ** sock_list
 *  @return : int   # max socket descriptor number
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
 *  sig_chld
 *
 *  SIGCHLD Signal Handler
 *
 *  @param  : int signo
 *  @return : void
 *  @see    : struct#process_info
 *
 *  Catch SIGCHLD signal
 *  Remove the items in process_info structure and terminate all the zombie
 *  children
 * --------------------------------------------------------------------------
 */
void sig_chld(int signo) {
    pid_t   pid;
    int     stat;
    struct process_info *proc_prev = NULL;

    while ((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
        // remove the entry from process_info
        proc = proc_head;
        while (proc != NULL) {
            if (proc->pid == pid) {
                if (proc == proc_head)
                    proc_head = proc->next;
                else
                    proc_prev->next = proc->next;
                free(proc);
                break;
            }
            proc_prev = proc;
            proc = proc->next;
        }

        printf("[Server]: Child %d terminated.\n", pid);
    }
    return;
}

/* --------------------------------------------------------------------------
 *  checkProcess
 *
 *  File request check function
 *
 *  @param  : char  *filename
 *            char  *address
 *            int   port
 *  @return : int   # 0 if the request is new
 *                  # otherwise, return the process id
 *
 *  Check if the file request is already handled by a child process
 * --------------------------------------------------------------------------
 */
int checkProcess(char *filename, char *address, int port) {
    proc = proc_head;

    while (proc != NULL) {
        if ((strcmp(proc->filename, filename) == 0) && (strcmp(proc->address, address) == 0) && proc->port == port)
            return proc->pid;
        proc = proc->next;
    }
    return 0;
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
    int         maxfdp1 = -1, r, len;
    pid_t       childpid;
    fd_set      rset;
    struct socket_info  *sock_head = NULL, *sock = NULL;
    struct sockaddr     clientfrom;
    struct filedatagram datagram;

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

    // use function sig_chld as SIGCHLD handler, function sig_int as SIGINT handler
    Signal(SIGCHLD, sig_chld);

    FD_ZERO(&rset);
    len = sizeof(clientfrom);
    for ( ; ; ) {
        // use select() to monitor all listening sockets
        for (sock = sock_head; sock != NULL; sock = sock->next)
            FD_SET(sock->sockfd, &rset);

        // need to use select rather than Select provided by Steven
        // cos Steven's Select doesn't handle EINTR
        r = select(maxfdp1, &rset, NULL, NULL, NULL);

        // slow system call select() may be interrupted
        if (r == -1 && errno == EINTR)
            continue;

        // handle the readable socket
        for (sock = sock_head; sock != NULL; sock = sock->next) {
            if (FD_ISSET(sock->sockfd, &rset)) {

                // fill the packet datagram
                bzero(&datagram, sizeof(datagram));
                Dg_recvpacket(sock->sockfd, &clientfrom, &len, &datagram);

                // check the packet contains a filename
                if (datagram.flag.fln == 1) {
                    struct sockaddr_in *clientaddr_in = (struct sockaddr_in *)&clientfrom;
                    printf("[Server]: Received a valid file request \x1B[0;34m\"%s\"\x1B[0;0m from client \x1B[0;33m%s:%d\x1B[0;0m to server \x1B[0;33m%s:%d\x1B[0;0m\n",
                        datagram.data,
                        Sock_ntop_host(&clientfrom, len), clientaddr_in->sin_port,
                        Sock_ntop_host(sock->addr, sizeof(*(sock->addr))), port);

                    // check if the file request already handled
                    childpid = checkProcess(datagram.data, Sock_ntop_host(&clientfrom, len), clientaddr_in->sin_port);
                    if (childpid > 0) {
                        printf("[Server]: A duplicate file request already handled by child #%d.\n", childpid);
                        continue;
                    }

                    childpid = Fork();
                    if (childpid == 0) {
                        // this is child process part
                        Dg_serv(sock->sockfd, sock_head, sock->addr, &clientfrom, datagram.data, max_winsize);
                        exit(0);
                    } else {
                        // this is parent process part

                        // save the pid, filename, client IP and port to process_info
                        proc = Malloc(sizeof(struct process_info));
                        bzero(proc, sizeof(*proc));
                        proc->pid = childpid;
                        strcpy(proc->filename, datagram.data);
                        strcpy(proc->address, Sock_ntop_host(&clientfrom, len));
                        proc->port = clientaddr_in->sin_port;
                        proc->next = proc_head;
                        proc_head = proc;
                    }
                } else {
                    printf("[Server]: Received an invalid packet (no filename requested).\n");
                }
                continue;
            }
        }

    }

    exit(0);
}
