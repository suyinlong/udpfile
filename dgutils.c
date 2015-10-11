/*
* @Author: Yinlong Su
* @Date:   2015-10-09 20:43:25
* @Last Modified by:   Yinlong Su
* @Last Modified time: 2015-10-11 16:51:00
*
* File:         dgutils.c
* Description:  Datagram Utils C file
*/

#include "udpfile.h"

/* --------------------------------------------------------------------------
 *  Dg_sendpacket
 *
 *  Datagram sendto function
 *
 *  @param  : int sockfd,
 *            struct sockaddr *to,
 *            socklen_t addrlen,
 *            const struct filedatagram *datagram
 *  @return : void
 *
 *  For the unconnected socket, use sendto() to send packets
 * --------------------------------------------------------------------------
 */
void Dg_sendpacket(int sockfd, struct sockaddr *to, socklen_t addrlen, const struct filedatagram *datagram) {
    size_t n = DATAGRAM_HEADERSIZE + datagram->datalength;

    Sendto(sockfd, (char *)datagram, n, 0, to, addrlen);
}

/* --------------------------------------------------------------------------
 *  Dg_recvpacket
 *
 *  Datagram recvfrom function
 *
 *  @param  : int sockfd,
 *            struct sockaddr *from,
 *            socklen_t *addrlen,
 *            struct filedatagram *datagram
 *  @return : void
 *
 *  For the unconnected socket, use recvfrom() to fill out packet
 * --------------------------------------------------------------------------
 */
void Dg_recvpacket(int sockfd, struct sockaddr *from, socklen_t *addrlen, struct filedatagram *datagram) {
    char buff[DATAGRAM_PAYLOAD];

    bzero(datagram, DATAGRAM_PAYLOAD);
    size_t n = Recvfrom(sockfd, buff, DATAGRAM_PAYLOAD, 0, from, addrlen);
    memcpy(datagram, buff, n);
}

/* --------------------------------------------------------------------------
 *  Dg_writepacket
 *
 *  Datagram write function
 *
 *  @param  : int sockfd,
 *            const struct filedatagram *datagram
 *  @return : void
 *
 *  For the connected socket, use write() to send packets instead of
 *  sendto()
 * --------------------------------------------------------------------------
 */
void Dg_writepacket(int sockfd, const struct filedatagram *datagram) {
    size_t n = DATAGRAM_HEADERSIZE + datagram->datalength;

    Write(sockfd, (char *)datagram, n);
}

/* --------------------------------------------------------------------------
 *  Dg_readpacket
 *
 *  Datagram read function
 *
 *  @param  : int sockfd,
 *            struct filedatagram *datagram
 *  @return : void
 *
 *  For the connected socket, use read() to receive packets instead of
 *  recvfrom()
 * --------------------------------------------------------------------------
 */
void Dg_readpacket(int sockfd, struct filedatagram *datagram) {
    char buff[DATAGRAM_PAYLOAD];

    bzero(datagram, DATAGRAM_PAYLOAD);
    size_t n = Read(sockfd, buff, DATAGRAM_PAYLOAD);
    memcpy(datagram, buff, n);
}

unsigned int getDGSequence(struct filedatagram dg) {
    return dg.seq;
}

unsigned int getSequence(char *ptr) {
    return *((unsigned int *)ptr);
}

DATAGRAM_STATUS getDGFlag(struct filedatagram dg) {
    return dg.flag;
}

DATAGRAM_STATUS getFlag(char *ptr) {
    return *((DATAGRAM_STATUS *)(ptr + sizeof(unsigned int)));
}

char *getDGData(struct filedatagram dg) {
    return dg.data;
}

char *getData(char *ptr) {
    return (ptr + sizeof(unsigned int) + sizeof(DATAGRAM_STATUS));
}

