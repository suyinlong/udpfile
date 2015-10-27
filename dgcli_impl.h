/**
* @file         :  dgcli_impl.h
* @author       :  Jiewen Zheng
* @date         :  2015-10-13
* @brief        :  UDP datagram client implementation
* @changelog    :
**/

#ifndef __DGCLI_IMPL_H_
#define __DGCLI_IMPL_H_

#include "udpfile.h"
#include "dgbuffer.h"

/**
* @brief Define client arguments
*/
typedef struct dg_arg_t
{
    char     srvIP[IP_BUFFSIZE];            // IP address of the server (not the hostname)
    int      srvPort;                       // well-known port number of server
    char     filename[FILENAME_BUFFSIZE];   // filename to be transferred
    int      rcvWin;                        // receiving sliding-window size (in number of datagrams)
    int      seed;                          // random generator seed value
    double   p;                             // probability p of datagram loss
    int      u;                             // an exponential distribution controlling the rate value
}dg_arg;

/**
* @brief Define client object
*/
typedef struct rtt_info dg_rtt;
typedef struct dg_client_t
{
    dg_arg     *arg;                // dg_arg object
    dg_fifo    *fifo;               // fifo object
    dg_rcv_buf *buf;                // receive buffer object
    dg_rtt      rtt;                // rtt object
    uint32_t    seq;                // client segment sequence
    timer_t     delayedAckTimer;    // delayed ack timer
    int         sock;               // UDP socket
    int         newPort;            // new port number of server
    int         timeout;            // time out value
    int         printSeq;           // print sequence flag, if 1 print on screen  
    int         printFile;          // print file content flag, if 1 print on screen
}dg_client;

/**
* @brief Create the client object
* @param[in] arg  : dg_arg object
* @param[in] sock : socket value
* @return client object if OK, NULL on error
**/
dg_client *CreateDgCli(const dg_arg *arg, int sock);

/**
* @brief Destroy the client object
* @param[in] cli : dg_cli object
**/
void DestroyDgCli(dg_client *cli);

/**
* @brief Initial the client
* @param[in] cli : dg_cli object
* @return 0 if OK, -1 on error
**/
int InitDgCli(dg_client *cli);

/**
* @brief Start the client
* @param[in] cli : dg_cli object
* @return  0 if OK, -1 error
**/
int StartDgCli(dg_client *cli);


#endif // __DGCLI_IMPL_H_

