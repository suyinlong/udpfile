/*
* @Author: Yinlong Su
* @Date:   2015-10-11 14:26:14
* @Last Modified by:   Yinlong Su
* @Last Modified time: 2015-10-21 17:48:49
*
* File:         dgserv.c
* Description:  Datagram Server C file
*/

#include "udpfile.h"

pid_t   pid;
char    IPserver[IP_BUFFSIZE], IPclient[IP_BUFFSIZE];
FILE    *fp;

uint32_t buff_seq = 0, serv_seq = 0;
struct sender_window *swnd_head = NULL, *swnd_now = NULL, *swnd_tail = NULL;

/* --------------------------------------------------------------------------
 *  Dg_cli_read
 *
 *  Server datagram read function
 *
 *  @param  : int                   sockfd
 *            struct filedatagram   *datagram
 *  @return : void
 *  @see    :
 *
 *  For connected socket
 * --------------------------------------------------------------------------
 */
void Dg_serv_read(int sockfd, struct filedatagram *datagram) {
    Dg_readpacket(sockfd, datagram);
}

/* --------------------------------------------------------------------------
 *  Dg_serv_write
 *
 *  Server datagram write function
 *
 *  @param  : int                   sockfd
 *            struct filedatagram   *datagram
 *  @return : void
 *  @see    :
 *
 *  For connected socket
 * --------------------------------------------------------------------------
 */
void Dg_serv_write(int sockfd, struct filedatagram *datagram) {
    datagram->ts = 0; // modify this number and adjust this line just before the packet send into window

    // TODO: sender window buffer
    // TODO: retransmission
    // TODO: others



    Dg_writepacket(sockfd, datagram);
}

/* --------------------------------------------------------------------------
 *  Dg_serv_send
 *
 *  Server datagram send function
 *
 *  @param  : int                   sockfd
 *            const sockaddr_in     *cliaddr
 *            socklen_t             clilen
 *            struct filedatagram   *datagram
 *  @return : void
 *  @see    :
 *
 *  For unconnected socket
 * --------------------------------------------------------------------------
 */
void Dg_serv_send(int sockfd, const SA* cliaddr, socklen_t clilen, struct filedatagram *datagram) {
    datagram->ts = 0; // modify this number and adjust this line just before the packet send into window

    // TODO: sender window buffer
    // TODO: retransmission
    // TODO: others

    Dg_sendpacket(sockfd, cliaddr, clilen, datagram);
}

/* --------------------------------------------------------------------------
 *  Dg_serv_send
 *
 *  Server datagram send function
 *
 *  @param  : struct socket_info *  sock_head
 *            struct sockaddr *     server
 *            struct sockaddr *     client
 *  @return : int
 *  @see    :
 *
 *  Check if the server and client are local
 * --------------------------------------------------------------------------
 */
int checkLocal(struct socket_info *sock_head, struct sockaddr *server, struct sockaddr *client) {
    int local = 0;
    unsigned long   ntm, subnserver, subnclient;
    struct socket_info      *sock = NULL;

    bzero(IPserver, IP_BUFFSIZE);
    strcpy(IPserver, Sock_ntop_host(server, sizeof(*server)));
    bzero(IPclient, IP_BUFFSIZE);
    strcpy(IPclient, Sock_ntop_host(client, sizeof(*client)));

    // check if local (loopback)
    if (strcmp(IPserver, "127.0.0.1") == 0)
        local = 1;

    // check if local (subnets)
    for (sock = sock_head; sock != NULL; sock = sock->next)
        if (sock->addr == server) {
            ntm = inet_addr(Sock_ntop_host(sock->ntmaddr, sizeof(*(sock->ntmaddr))));
            subnserver = inet_addr(IPserver) & ntm;
            subnclient = inet_addr(IPclient) & ntm;
            if (subnserver == subnclient) {
                local = 1;
                break;
            }
        }

    // print out the information
    printf("[Server Child #%d]: Client %s and Server %s is ", pid, IPclient, IPserver);
    if (local)
        printf("local.\n");
    else
        printf("not local.\n");

    return local;
}

/* --------------------------------------------------------------------------
 *  Dg_serv_buffer
 *
 *  Server window buffer function
 *
 *  @param  : int       size    # indicate buffer [size] more packets
 *  @return : void
 *
 *  Buffer datagrams in the sender window buffer
 * --------------------------------------------------------------------------
 */
void Dg_serv_buffer(int size) {
    int i;
    struct sender_window *swnd;

    for (i = 0; i < size; i++) {
         // return if EOF
        if (feof(fp)) return;

        // malloc memory
        swnd = Malloc(sizeof(struct sender_window));
        bzero(&swnd->datagram, DATAGRAM_PAYLOAD);
        swnd->next = NULL;

        // fill the datagram
        swnd->datagram.seq = ++ buff_seq;
        swnd->datagram.len = fread(swnd->datagram.data, sizeof(char), DATAGRAM_DATASIZE, fp);
        if (feof(fp))
            swnd->datagram.flag.eof = 1;

        // modify former tail's next
        if (swnd_tail)
            swnd_tail->next = swnd;
        // modify tail
        swnd_tail = swnd;
        // set head if head is NULL
        if (swnd_head == NULL)
            swnd_head = swnd;
    }
}

/* --------------------------------------------------------------------------
 *  Dg_serv_file
 *
 *  Server file send function
 *
 *  @param  : int       sockfd
 *            char *    filename
 *  @return : void
 *  @see    :
 *
 *  This function is used for sending file content
 * --------------------------------------------------------------------------
 */
void Dg_serv_file(int sockfd, char *filename, int max_winsize) {
    int k;
    char l;
    struct sender_window *swnd;
    struct filedatagram FD;

    fp = Fopen(filename, "r+t");

    // fill the buffer with max_winsize
    Dg_serv_buffer(max_winsize);

    // start to send packet
    swnd_now = swnd_head;

    // TODO: need to change the process
    // 1. call cc_wnd to get windowsize
    // 2. determine whether there is a lost datagram need to be retransmitted
    // 3. retransmit if needed, start timer wait for ack
    // 4. if no lost datagram, transmit datagrams (according to swnd awnd cwnd)
    // 5. wait for ack
    // 6. ack received, sliding windows(if needed), call cc_ack, update awnd
    // 7. if awnd = 0, start persist timer, wait ack to update awnd or send probe when timer expires
    // 8. else to 1
    while (swnd_now) {
        printf("[Server Child #%d]: Send datagram #%d.\n", pid, swnd_now->datagram.seq);
        Dg_serv_write(sockfd, &swnd_now->datagram);
        swnd_now = swnd_now->next;
        Dg_serv_read(sockfd, &FD);

        printf("[Server Child #%d]: Received ACK #%d.\n", pid, FD.ack);
        //cc_ack(FD.seq, FD.ts, FD.wnd);

        // free ACKed datagram from head
        swnd = swnd_head;
        k = 0;
        while (swnd && swnd->datagram.seq < FD.ack) {
            k++;

            // after free, head should move forward
            // if the tail is ACKed, set the tail to NULL
            swnd_head = swnd->next;
            if (swnd_tail == swnd)
                swnd_tail = NULL;
            //printf("[Server Child #%d]: Free datagram #%d, k=%d, next=%d.\n", pid, swnd->datagram.seq, k, swnd->next);
            free(swnd);

            // try next datagram, start from head
            swnd = swnd_head;
        }

        // printf("[Server Child #%d]: Call buffer %d.\n", pid, k);
        Dg_serv_buffer(k);

        // after rebuffer, check if there is some data need to send
        if (swnd_head)
            swnd_now = swnd_head;
    }
    Fclose(fp);
}

/* --------------------------------------------------------------------------
 *  Dg_serv
 *
 *  Server service function
 *
 *  @param  : int                   listeningsockfd
 *            struct socket_info *  sock_head
 *            struct sockaddr *     server
 *            struct sockaddr *     client
 *            char *                filename
 *  @return : void
 *  @see    :
 *
 *  Create new socket
 *  Send private port number
 * --------------------------------------------------------------------------
 */
void Dg_serv(int listeningsockfd, struct socket_info *sock_head, struct sockaddr *server, struct sockaddr *client, char *filename, int max_winsize) {
    int             local = 0, sockfd, len;
    char            s_port[6];
    const int       on = 1;
    struct filedatagram     FD;
    struct sockaddr_in      servaddr;
    struct sockaddr_storage ss;
    struct socket_info      *sock = NULL;


    pid = getpid();

    // close all socket except 'listening' socket
    for (sock = sock_head; sock != NULL; sock = sock->next)
        if (sock->sockfd != listeningsockfd) close(sock->sockfd);

    // check if local
    local = checkLocal(sock_head, server, client);

    // create new socket
    sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
    if (local)
        Setsockopt(sockfd, SOL_SOCKET, SO_DONTROUTE, &on, sizeof(on));

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    //servaddr.sin_port = htons(0);
    Inet_pton(AF_INET, IPserver, &servaddr.sin_addr);

    // bind the servaddr to socket
    Bind(sockfd, (SA *)&servaddr, sizeof(servaddr));

    // getsockname of server part
    len = sizeof(ss);
    if (getsockname(sockfd, (SA *) &ss, &len) < 0) {
        printf("getsockname error\n");
        exit(-1);
    }

    // output socket information
    struct sockaddr_in *sockaddr = (struct sockaddr_in *)&ss;
    printf("[Server Child #%d]: UDP Server Socket (with new private port): \x1B[0;33m%s:%d\x1B[0;0m\n", pid, inet_ntoa(sockaddr->sin_addr), sockaddr->sin_port);

    // connect
    Connect(sockfd, client, sizeof(*client));

    // create a datagram packet with port number
    bzero(&FD, sizeof(FD));
    FD.seq = serv_seq;
    FD.ack = 1;
    FD.flag.pot = 1;    // indicate this is a packet with port number
    sprintf(s_port, "%d", sockaddr->sin_port);
    FD.len = strlen(s_port);
    strcpy(FD.data, s_port);

    // send the new private port number via listening socket
    Dg_serv_send(listeningsockfd, client, sizeof(*client), &FD);

    // should receive ACK from connection socket
    Dg_serv_read(sockfd, &FD);

    if (FD.ack == 1 && FD.flag.pot == 1) {
        printf("[Server Child #%d]: Received ACK. Private connection established.\n", pid);
        close(listeningsockfd);
    } else {
        printf("[Server Child #%d]: Received an invalid packet (not ACK).\n");
    }

    // start to transfer file content

    Dg_serv_file(sockfd, filename, max_winsize);

    printf("[Server Child #%d]: Finish sending file.\n", pid);
}
