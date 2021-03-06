/*
* @Author: Yinlong Su
* @Date:   2015-10-11 14:26:14
* @Last Modified by:   Yinlong Su
* @Last Modified time: 2015-10-27 22:27:08
*
* File:         dgserv.c
* Description:  Datagram Server C file
*/

#include "udpfile.h"

int     pfd[2];
pid_t   pid;
char    IPserver[IP_BUFFSIZE], IPclient[IP_BUFFSIZE];
FILE    *fp;

char    rttinit = 0;
struct rtt_info rttinfo;
uint32_t buff_seq = 0;
struct sender_window *swnd_head = NULL, *swnd_now = NULL, *swnd_tail = NULL;

extern uint8_t rto_display;

/* --------------------------------------------------------------------------
 *  Dg_cli_read
 *
 *  Server datagram read function
 *
 *  @param  : int                   sockfd
 *            struct filedatagram   *datagram
 *  @return : int   # -1 if read error
 *                  # otherwise, return the length of bytes read
 *  @see    : function#Dg_readpacket
 *
 *  For connected socket
 * --------------------------------------------------------------------------
 */
int Dg_serv_read(int sockfd, struct filedatagram *datagram) {
    return Dg_readpacket(sockfd, datagram);
}

/* --------------------------------------------------------------------------
 *  Dg_cli_read_nb
 *
 *  Server datagram read function (Non-blocking)
 *
 *  @param  : int                   sockfd
 *            struct filedatagram   *datagram
 *  @return : int   # -1 if read error
 *                  # otherwise, return the length of bytes read
 *  @see    : function#Dg_readpacket_nb
 *
 *  For connected socket
 * --------------------------------------------------------------------------
 */
int Dg_serv_read_nb(int sockfd, struct filedatagram *datagram) {
    return Dg_readpacket_nb(sockfd, datagram);
}

/* --------------------------------------------------------------------------
 *  Dg_serv_write
 *
 *  Server datagram write function
 *
 *  @param  : int                   sockfd
 *            struct filedatagram   *datagram
 *  @return : void
 *  @see    : function#Dg_writepacket
 *
 *  For connected socket, fill the timestamp and send it
 * --------------------------------------------------------------------------
 */
void Dg_serv_write(int sockfd, struct filedatagram *datagram) {
    datagram->ts = rtt_ts(&rttinfo);
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
 *  @see    : function#Dg_sendpacket
 *
 *  For unconnected socket, fill the timestamp and send it
 * --------------------------------------------------------------------------
 */
void Dg_serv_send(int sockfd, const SA* cliaddr, socklen_t clilen, struct filedatagram *datagram) {
    datagram->ts = rtt_ts(&rttinfo);
    Dg_sendpacket(sockfd, cliaddr, clilen, datagram);
}

/* --------------------------------------------------------------------------
 *  sig_alrm
 *
 *  Server datagram timeout handler
 *
 *  @param  : int   signo
 *  @return : void
 *
 *  Write one byte to the pipe, inform select() that the oldest datagram
 *  is timeout
 * --------------------------------------------------------------------------
 */
static void sig_alrm(int signo) {
    char c;
    Write(pfd[1], &c, 1);
}

/* --------------------------------------------------------------------------
 *  setAlarm
 *
 *  Server datagram timeout alarm
 *
 *  @param  : uint32_t  ms  # in milliseconds
 *  @return : void
 *
 *  Inline function for timeout alarm, use setitimer() instead of alarm()
 * --------------------------------------------------------------------------
 */
static inline void setAlarm(uint32_t ms) {
    struct itimerval t;
    t.it_value.tv_sec = ms / 1000;
    t.it_value.tv_usec = (ms % 1000) * 1000;
    t.it_interval.tv_sec = 0;
    t.it_interval.tv_usec = 0;

    setitimer(ITIMER_REAL, &t, NULL);
}

/* --------------------------------------------------------------------------
 *  checkLocal
 *
 *  Server local judge function
 *
 *  @param  : struct socket_info    *sock_head
 *            struct sockaddr       *server
 *            struct sockaddr       *client
 *  @return : int   # 1 if the server and the client are local
 *                  # 0 if otherwise
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
 *  @see    : struct#sender_window
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
        // set now if now is NULL
        if (swnd_now == NULL)
            swnd_now = swnd;
    }
}

/* --------------------------------------------------------------------------
 *  Dg_serv_ack
 *
 *  Server ACK handle function
 *
 *  @param  : int       sockfd
 *  @return : uint32_t  # max ack number
 *
 *  Receive datagram (ACK) and update RTO, cwnd and sliding window
 *  Set the socket to non-blocking to handle all received ACK
 *  Fast retransmission if needed
 * --------------------------------------------------------------------------
 */
uint32_t Dg_serv_ack(int sockfd) {
    int flag, k = 0;
    uint8_t     fr_flag = 0; // fast restransmission flag
    uint32_t    max_ack = 0; // max ack number
    struct sender_window *swnd;
    struct filedatagram FD;

    // set to nonblocking
    flag = Fcntl(sockfd, F_GETFL, 0);
    Fcntl(sockfd, F_SETFL, flag | FNDELAY);

    while ((flag = Dg_serv_read_nb(sockfd, &FD)) >= 0) {
        if (FD.ack > swnd_head->datagram.seq)
            setAlarm(0);
        max_ack = max(max_ack, FD.ack);

        printf("[Server Child #%d]: Received ACK #%d, awnd = %d", pid, FD.ack, FD.wnd);
        if (FD.flag.wnd)
            printf(" <WNDUPD>");

        if (FD.ts > 0) {
            printf(", rtt = %d", rtt_ts(&rttinfo) - FD.ts);
            rtt_stop(&rttinfo, rtt_ts(&rttinfo) - FD.ts);
        }

        cc_ack(FD.ack, FD.wnd, FD.flag.wnd, &fr_flag);

        if (fr_flag) {
            Dg_serv_write(sockfd, &swnd_head->datagram);
            if (isatty(fileno(stdout)))
                printf("[Server Child #%d]: Resend datagram #%d \x1b[43;31m(Fast Retransmission)\x1B[0;0m.\n", pid, swnd_head->datagram.seq);
            else
                printf("[Server Child #%d]: Resend datagram #%d (Fast Retransmission).\n", pid, swnd_head->datagram.seq);
        }

        // free ACKed datagram from head
        swnd = swnd_head;
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

    }

    // set to blocking
    flag = Fcntl(sockfd, F_GETFL, 0);
    Fcntl(sockfd, F_SETFL, flag & ~FNDELAY);

    // printf("[Server Child #%d]: Call buffer %d.\n", pid, k);
    Dg_serv_buffer(k);
    return max_ack;
}

/* --------------------------------------------------------------------------
 *  probeClientWindow
 *
 *  Server window probe function
 *
 *  @param  : int       sockfd
 *  @return : uint16_t  # rwnd/awnd receiver advertised window size
 *
 *  Sending window update probe to client, the interval is PERSIST_TIMER
 * --------------------------------------------------------------------------
 */
uint16_t probeClientWindow(int sockfd) {
    int     r;
    char    c;
    fd_set  fds;
    struct filedatagram FD;

    bzero(&FD, sizeof(FD));
    FD.flag.pob = 1;    // indicate this is a probe packet

probeagain:
    Dg_serv_write(sockfd, &FD);
    setAlarm(PERSIST_TIMER);
    printf("[Server Child #%d]: Send window probe.\n", pid);

    for ( ; ; ) {
        FD_ZERO(&fds);
        FD_SET(sockfd, &fds);
        FD_SET(pfd[0], &fds);

        r = select(max(sockfd, pfd[0]) + 1, &fds, NULL, NULL, NULL);
        if (r == -1 && errno == EINTR)
            continue;
        if (FD_ISSET(sockfd, &fds)) {
            // datagram received
            Dg_serv_ack(sockfd);
            if (cc_wnd() > 0)
                return cc_wnd();
        } else if (FD_ISSET(pfd[0], &fds)) {
            // timeout
            Read(pfd[0], &c, 1);
            goto probeagain;
        }
        if (r == -1)
            err_sys("select error");
    }
    return 0;
}

/* --------------------------------------------------------------------------
 *  Dg_serv_file
 *
 *  Server file send function
 *
 *  @param  : int       sockfd
 *            char *    filename
 *            int       max_winsize
 *            int       rwnd
 *  @return : int       # 0 = fail
 *
 *  1. Initialize:
 *      a. Open the requested file
 *      b. Buffer the sender window
 *      c. Init Congestion Control arguments
 *  2. Sending file contents
 *      a. Call cc_wnd, get the number of datagrams can be sent one time
 *      b. If the awnd is 0, call probeClientWindow to probe window update
 *      c. Ready to send new datagram, call rtt_newpack
 *      d. Send datagrams in order, set timer if needed
 *      e. Use select to monitor the socket and the pipe, resend the datagram
 *         if timeout. Exit if run out of retry number.
 *      f. Call Dg_serv_ack to process ACK
 *      g. If there is other datagram to send, go to step.a
 *      h. Close file and exit
 * --------------------------------------------------------------------------
 */
int Dg_serv_file(int sockfd, char *filename, int max_winsize, int rwnd) {
    int     r;
    char    c;
    fd_set  fds;
    char        alarm_set       = 0; // alarm set flag
    uint16_t    max_sendsize    = 0;
    uint32_t    min_seq, max_seq;

    fp = Fopen(filename, "r+t");

    // fill the buffer with max_winsize
    Dg_serv_buffer(max_winsize);

    // init congestion control
    cc_init(rwnd, max_winsize);

    // start to send packet
    swnd_now = swnd_head;

    while (1) {
        alarm_set = 0;
        min_seq = 0xFFFFFFFF;
        max_seq = 0;
        max_sendsize = cc_wnd();

        // if awnd=0 send probe to get window update
        if (max_sendsize == 0)
            max_sendsize = probeClientWindow(sockfd);

        // Set rtt newpack if there is at least one packet need to send
        if (max_sendsize > 0)
            rtt_newpack(&rttinfo);

        // can only transmit cc_wnd() datagrams from swnd_head: now.seq < head.seq + cc_wnd()
        while (swnd_now && swnd_now->datagram.seq < swnd_head->datagram.seq + max_sendsize) {
            // after (possible) retransmit, if sendsize > 0, send more datagrams
            Dg_serv_write(sockfd, &swnd_now->datagram);
            min_seq = min(swnd_now->datagram.seq, min_seq);
            max_seq = max(swnd_now->datagram.seq, max_seq);

            // Set alarm for the oldest datagram
            if (alarm_set == 0) {
                setAlarm(rtt_start(&rttinfo));
                alarm_set = 1;
            }
            swnd_now = swnd_now->next;
        }

        if (max_seq > 0)
            printf("[Server Child #%d]: Send datagram #%d - #%d.\n", pid, min_seq, max_seq);

selectagain:
        if (alarm_set == 0) {
            setAlarm(rtt_start(&rttinfo));
            alarm_set = 1;
        }
        // loop for select
        for ( ; ; ) {
            FD_ZERO(&fds);
            FD_SET(sockfd, &fds);
            FD_SET(pfd[0], &fds);

            r = select(max(sockfd, pfd[0]) + 1, &fds, NULL, NULL, NULL);
            if (r == -1 && errno == EINTR)
                continue;
            if (FD_ISSET(sockfd, &fds)) {
                // datagram received
                uint32_t oldseq = swnd_head->datagram.seq;
                if (Dg_serv_ack(sockfd) > oldseq)
                    break;
            } else if (FD_ISSET(pfd[0], &fds)) {
                // timeout
                Read(pfd[0], &c, 1);
                if (rtt_timeout(&rttinfo) < 0) {
                    if (isatty(fileno(stdout)))
                        printf("[Server Child #%d]: \x1b[41;33mTerminate for file datagram timeout.\x1B[0;0m\n", pid);
                    else
                        printf("[Server Child #%d]: Terminate for file datagram timeout.\n", pid);
                    rttinit = 0;
                    errno = ETIMEDOUT;
                    return 0;
                }
                cc_timeout();
                Dg_serv_write(sockfd, &swnd_head->datagram);
                setAlarm(rtt_start(&rttinfo));
                if (isatty(fileno(stdout)))
                    printf("[Server Child #%d]: Resend datagram #%d \x1b[43;31m(Timeout #%2d)\x1B[0;0m.\n", pid, swnd_head->datagram.seq, rttinfo.rtt_nrexmt);
                else
                    printf("[Server Child #%d]: Resend datagram #%d (Timeout #%2d).\n", pid, swnd_head->datagram.seq, rttinfo.rtt_nrexmt);
                goto selectagain;
            }
            if (r == -1)
                err_sys("select error");
        }
        // check if there is some data need to send
        if (swnd_head == NULL)
            break;

    }
    Fclose(fp);
    return 1;
}

/* --------------------------------------------------------------------------
 *  Dg_serv_port
 *
 *  Server port number send function
 *
 *  @param  : int               port
 *            int               listeningsockfd # old socket
 *            int               sockfd          # new private port socket
 *            struct sockaddr   *client
 *  @return : int               # -1 = fail
 *
 *  Use RTO mechanism to send port number
 *  If timeout, retry by sending port number to both listeningsockfd and
 *  sockfd
 * --------------------------------------------------------------------------
 */
int Dg_serv_port(int port, int listeningsockfd, int sockfd, struct sockaddr *client) {
    char    c, s_port[10], port_retry = 0;
    int     r;
    fd_set  fds;
    struct filedatagram portFD, FD;

    printf("[Server Child #%d]: Waiting for port number acknowledged from client...\n", pid);

    rtt_newpack(&rttinfo); // new packet

    // create a datagram packet with port number
    bzero(&portFD, sizeof(portFD));
    portFD.seq = 0;         // port datagram has seq = 0
    portFD.ack = 1;
    portFD.flag.pot = 1;    // indicate this is a packet with port number
    sprintf(s_port, "%d", port);
    portFD.len = strlen(s_port);
    strcpy(portFD.data, s_port);

sendportagain:
    // send the new private port number via listening socket (and connected socket, if timeout)
    Dg_serv_send(listeningsockfd, client, sizeof(*client), &portFD);
    if (port_retry > 0) {
        Dg_serv_write(sockfd, &portFD);
        if (isatty(fileno(stdout)))
            printf("[Server Child #%d]: Resend port number %s \x1b[42;30m(Timeout #%2d)\x1B[0;0m.\n", pid, portFD.data, port_retry);
        else
            printf("[Server Child #%d]: Resend port number %s (Timeout #%2d).\n", pid, portFD.data, port_retry);
    }
    port_retry++;
    setAlarm(rtt_start(&rttinfo));

    for ( ; ; ) {

        FD_ZERO(&fds);
        FD_SET(sockfd, &fds);
        FD_SET(pfd[0], &fds);

        r = select(max(sockfd, pfd[0]) + 1, &fds, NULL, NULL, NULL);

        if (r == -1 && errno == EINTR)
            continue;
        if (FD_ISSET(sockfd, &fds)) {
            // datagram received, should receive ACK from connection socket
            if (Dg_serv_read(sockfd, &FD) < 0)
                continue;
            setAlarm(0);
            if (FD.ts > 0)
                rtt_stop(&rttinfo, rtt_ts(&rttinfo) - FD.ts);
            if (FD.ack == 1 && FD.flag.pot == 1) {
                printf("[Server Child #%d]: Received ACK. Private connection established.\n", pid);
                close(listeningsockfd);
                return (FD.wnd);
            } else {
                printf("[Server Child #%d]: Received an invalid packet (not port ACK).\n", pid);
                goto sendportagain;
            }

        } else if (FD_ISSET(pfd[0], &fds)) {
            // timeout
            Read(pfd[0], &c, 1);
            if (rtt_timeout(&rttinfo) < 0) {
                if (isatty(fileno(stdout)))
                    printf("[Server Child #%d]: \x1b[41;33mTerminate for port datagram timeout.\x1B[0;0m\n", pid);
                else
                    printf("[Server Child #%d]: Terminate for port datagram timeout.\n", pid);
                rttinit = 0;
                errno = ETIMEDOUT;
                break;
            }
            goto sendportagain;
        }
        if (r == -1)
            err_sys("select error");
    }
    return -1;
}

/* --------------------------------------------------------------------------
 *  Dg_serv
 *
 *  Server service function
 *
 *  @param  : int                   listeningsockfd
 *            struct socket_info    *sock_head
 *            struct sockaddr       *server
 *            struct sockaddr       *client
 *            char                  *filename
 *  @return : void
 *
 *  Create new socket on new port number
 *  Init rtt
 *  Send private port number
 * --------------------------------------------------------------------------
 */
void Dg_serv(int listeningsockfd, struct socket_info *sock_head, struct sockaddr *server, struct sockaddr *client, char *filename, int max_winsize) {
    int             local = 0, sockfd, len, rwnd;
    const int       on = 1;
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
    if (isatty(fileno(stdout)))
        printf("[Server Child #%d]: UDP Server Socket (with new private port): \x1B[0;33m%s:%d\x1B[0;0m\n", pid, inet_ntoa(sockaddr->sin_addr), sockaddr->sin_port);
    else
        printf("[Server Child #%d]: UDP Server Socket (with new private port): %s:%d\n", pid, inet_ntoa(sockaddr->sin_addr), sockaddr->sin_port);

    // connect
    Connect(sockfd, client, sizeof(*client));

    // init rtt
    if (rttinit == 0) {
        rtt_init(&rttinfo);
        rto_display = 1;
        rttinit = 1;
    }
    Signal(SIGALRM, sig_alrm); // Signal handler
    Pipe(pfd); // create pipe

    // start to transfer port number
    if ((rwnd = Dg_serv_port(sockaddr->sin_port, listeningsockfd, sockfd, client)) >= 0) {
        // start to transfer file content
        if (Dg_serv_file(sockfd, filename, max_winsize, rwnd) == 1)
            printf("[Server Child #%d]: Finish sending file.\n", pid);
        else
            printf("[Server Child #%d]: Sending file error.\n", pid);
    } else
        printf("[Server Child #%d]: Sending port number error.\n", pid);

    close(pfd[0]);
    close(pfd[1]);
}
