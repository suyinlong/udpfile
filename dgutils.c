/*
* @Author: Yinlong Su
* @Date:   2015-10-09 20:43:25
* @Last Modified by:   Yinlong Su
* @Last Modified time: 2015-10-27 18:35:39
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
 *  @param  : int                       sockfd,
 *            struct sockaddr           *to,
 *            socklen_t                 addrlen,
 *            const struct filedatagram *datagram
 *  @return : void
 *
 *  For the unconnected socket, use sendto() to send packets
 * --------------------------------------------------------------------------
 */
void Dg_sendpacket(int sockfd, struct sockaddr *to, socklen_t addrlen, const struct filedatagram *datagram) {
    int n = DATAGRAM_HEADERSIZE + datagram->len;

    Sendto(sockfd, (char *)datagram, n, 0, to, addrlen);
}

/* --------------------------------------------------------------------------
 *  Dg_recvpacket
 *
 *  Datagram recvfrom function
 *
 *  @param  : int                   sockfd,
 *            struct sockaddr       *from,
 *            socklen_t             *addrlen,
 *            struct filedatagram   *datagram
 *  @return : void
 *
 *  For the unconnected socket, use recvfrom() to fill out packet
 * --------------------------------------------------------------------------
 */
void Dg_recvpacket(int sockfd, struct sockaddr *from, socklen_t *addrlen, struct filedatagram *datagram) {
    char buff[DATAGRAM_PAYLOAD];

    bzero(datagram, DATAGRAM_PAYLOAD);
    int n = Recvfrom(sockfd, buff, DATAGRAM_PAYLOAD, 0, from, addrlen);
    memcpy(datagram, buff, n);
}

/* --------------------------------------------------------------------------
 *  Dg_writepacket
 *
 *  Datagram write function
 *
 *  @param  : int                       sockfd,
 *            const struct filedatagram *datagram
 *  @return : void
 *
 *  For the connected socket, use write() to send packets instead of
 *  sendto()
 * --------------------------------------------------------------------------
 */
void Dg_writepacket(int sockfd, const struct filedatagram *datagram) {
    int n = DATAGRAM_HEADERSIZE + datagram->len;

    Write(sockfd, (char *)datagram, n);
}

/* --------------------------------------------------------------------------
 *  Dg_readpacket
 *
 *  Datagram read function
 *
 *  @param  : int                   sockfd,
 *            struct filedatagram   *datagram
 *  @return : void
 *
 *  For the connected socket, use read() to receive packets instead of
 *  recvfrom()
 * --------------------------------------------------------------------------
 */
int Dg_readpacket(int sockfd, struct filedatagram *datagram) {
    int n;
    char buff[DATAGRAM_PAYLOAD];

    bzero(datagram, DATAGRAM_PAYLOAD);

Dg_readpacket_again:
    n = read(sockfd, buff, DATAGRAM_PAYLOAD);
    if (n >= 0) {
        memcpy(datagram, buff, n);
        return n;
    } else if (errno == ECONNREFUSED)
        goto Dg_readpacket_again;

    return n;
}

/* --------------------------------------------------------------------------
 *  Dg_readpacket_nb
 *
 *  Datagram read function (Non-blocking)
 *
 *  @param  : int                   sockfd,
 *            struct filedatagram   *datagram
 *  @return : int   # -1 if read error
 *                  # otherwise, return the length of bytes read
 *
 *  For the connected socket, use read() to receive packets instead of
 *  recvfrom()
 * --------------------------------------------------------------------------
 */
int Dg_readpacket_nb(int sockfd, struct filedatagram *datagram) {
    char buff[DATAGRAM_PAYLOAD];

    bzero(datagram, DATAGRAM_PAYLOAD);
    int n = read(sockfd, buff, DATAGRAM_PAYLOAD);
    if (n >= 0)
        memcpy(datagram, buff, n);
    return n;
}
