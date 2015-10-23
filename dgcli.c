/*
* @Author: Yinlong Su
* @Date:   2015-10-11 11:50:14
* @Last Modified by:   Yinlong Su
* @Last Modified time: 2015-10-23 00:28:38
*
* File:         dgcli.c
* Description:  Datagram Client C file
*/

#include "udpfile.h"

extern char     IPserver[IP_BUFFSIZE], IPclient[IP_BUFFSIZE];
extern char     filename[FILENAME_BUFFSIZE];
extern int      max_winsize;
extern int      seed;
extern double   p;
extern int      mu;

uint32_t        cli_seq = 0;

/* --------------------------------------------------------------------------
 *  Dg_cli_write
 *
 *  Client datagram write function
 *
 *  @param  : int                   sockfd
 *            struct filedatagram   *datagram
 *  @return : void
 *  @see    :
 *
 *  For connected socket
 * --------------------------------------------------------------------------
 */
void Dg_cli_write(int sockfd, struct filedatagram *datagram) {
    datagram->seq = cli_seq++;
    datagram->ts = 0; // modify this number and adjust this line just before the packet send into window

    // TODO: sender window buffer
    // TODO: retransmission
    // TODO: and others



    // if random() <= p, discard the datagram (just don't send)
    if (random() > p)
        Dg_writepacket(sockfd, datagram);
}

/* --------------------------------------------------------------------------
 *  Dg_cli_read
 *
 *  Client datagram read function
 *
 *  @param  : int                   sockfd
 *            struct filedatagram   *datagram
 *  @return : void
 *  @see    :
 *
 *  For connected socket
 * --------------------------------------------------------------------------
 */
void Dg_cli_read(int sockfd, struct filedatagram *datagram) {
readagain:
    Dg_readpacket(sockfd, datagram);

    // if random() <= p, discard the datagram and try to read again
    if (random() <= p)
        goto readagain;
}

/* --------------------------------------------------------------------------
 *  Dg_cli_file
 *
 *  Client file receive function
 *
 *  @param  : int   sockfd
 *  @return : void
 *  @see    :
 *
 *  This function is used for unpack datagram and send back ACK
 * --------------------------------------------------------------------------
 */
void Dg_cli_file(int sockfd) {
    int i, seq, timestamp, eof = 0;
    struct filedatagram FD;

    cli_seq = 2;
    for ( ; ; ) {
        Dg_cli_read(sockfd, &FD);

        seq = FD.seq;
        timestamp = FD.ts;
        eof = FD.flag.eof;

        // TODO: save to buffer
        // TODO: use thread to print out the content
        for (i = 0; i < FD.len; i++)
            printf("%c", FD.data[i]);

        bzero(&FD, DATAGRAM_PAYLOAD);
        FD.seq = cli_seq;
        FD.ack = seq + 1;
        FD.ts = timestamp;
        FD.wnd = max_winsize;

        Dg_cli_write(sockfd, &FD);

        if (eof)
            break;
    }
}

/* --------------------------------------------------------------------------
 *  Dg_cli
 *
 *  Client service function
 *
 *  @param  : int   sockfd
 *  @return : void
 *  @see    :
 *
 *  Initialize the random seed
 *  Send file request
 *  Reconnect to private connection
 * --------------------------------------------------------------------------
 */
void Dg_cli(int sockfd) {
    int new_port = 0;
    struct sockaddr_in  servaddr;
    struct filedatagram FD;

    // use seed to generate random
    srand((unsigned)seed);

    // create a datagram packet with filename
    bzero(&FD, sizeof(FD));
    FD.seq = 0;                 // seq = 0
    FD.flag.fln = 1;            // indicate this is a datagram including filename
    FD.len = strlen(filename);  // indicate the filename length
    strcpy(FD.data, filename);  // fill the data part

    // send the file request
    Dg_cli_write(sockfd, &FD);

    // receive the port number
    Dg_cli_read(sockfd, &FD);

    if (FD.flag.pot != 1 || FD.ack != 1) {
        printf("[Client]: Received an invalid packet (no port number).\n");
    } else {
        new_port = atoi(FD.data);
        printf("[Client]: Received a valid private port number %d from server.\n", new_port);
    }

    // reconnect
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(new_port);
    Inet_pton(AF_INET, IPserver, &servaddr.sin_addr);

    Connect(sockfd, (SA *)&servaddr, sizeof(servaddr));

    // create a datagram packet with ACK (port number)
    bzero(&FD, sizeof(FD));
    FD.seq = 1;
    FD.ack = 1;
    FD.wnd = max_winsize;
    FD.flag.pot = 1;
    FD.len = 0;

    // send the ACK
    Dg_cli_write(sockfd, &FD);

    // receive file content
    Dg_cli_file(sockfd);

    printf("Finish receiving file content.\n");
}
