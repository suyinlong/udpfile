/*
* @Author: Yinlong Su
* @Date:   2015-10-11 11:50:14
* @Last Modified by:   Yinlong Su
* @Last Modified time: 2015-10-11 13:33:12
*
* File:         dgcli.c
* Description:  Datagram Client C file
*/

#include "udpfile.h"

extern char     filename[FILENAME_BUFFSIZE];
extern int      max_winsize;
extern int      seed;
extern double   p;
extern int      mu;

unsigned int seq = 0;

void Dg_cli_send(int sockfd, const SA* servaddr, socklen_t servlen, struct filedatagram *datagram) {
    // set sequence number
    datagram->seq = seq++;
    datagram->timestamp = 0; // modify this number and adjust this line just before the packet send into window

    // TODO: sender window buffer
    // TODO: retransmission
    // TODO: others



    // if random() <= p=, discard the datagram
    if (random() > p)
        Dg_writepacket(sockfd, datagram);




}

void Dg_cli(int sockfd, const SA *servaddr, socklen_t servlen) {
    struct filedatagram FD;

    // use seed to generate random
    srand((unsigned)seed);

    bzero(&FD, sizeof(FD));
    FD.flag.fln = 1;                    // indicate this is a datagram including filename
    FD.datalength = strlen(filename);   // indicate the filename length
    strcpy(FD.data, filename);          // fill the data part

    Dg_cli_send(sockfd, servaddr, servlen, &FD);

}
