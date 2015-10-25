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
    int      srvPort;                       // Well-known port number of server
    char     filename[FILENAME_BUFFSIZE];   // filename to be transferred
    int      rcvWin;                        // Receiving sliding-window size (in number of datagrams)
    int      seed;                          // Random generator seed value
    double   p;                             // Probability p of datagram loss
    int      u;                             // An exponential distribution controlling the rate value
}dg_arg;

/**
* @brief Define client object
*/
typedef struct dg_client_t
{
    dg_arg     *arg;
    dg_fifo    *fifo;
    dg_rcv_buf *buf;
    uint32_t    seq;
    timer_t     delayedAckTimer;
    int         sock;
    int         newPort;
    int         timeout;    
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

