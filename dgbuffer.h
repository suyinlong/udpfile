/**
* @file         :  dgbuffer.h
* @author       :  Jiewen Zheng
* @date         :  2015-10-13
* @brief        :  Receive data buffer and FIFO implementation
* @changelog    :
**/

#ifndef __DG_BUFFER_H_
#define __DG_BUFFER_H_

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "unpthread.h"
#include "udpfile.h"

#define FIFO_SIZE   512

/**
* @brief Define node struct
*/
typedef struct dg_node_t
{
    int        size;
    char      *data;
    struct dg_node_t *next;
}dg_node;

/**
* @brief Define fifo struct
*/
typedef struct dg_fifo_t
{
    int      size;
    int      curSize;
    dg_node *head;
    dg_node *curData;
    pthread_mutex_t	mutex;
}dg_fifo;

/**
* @brief  Create fifo object
* @return  fifo object if OK, NULL on error
**/
dg_fifo *CreateDgFifo();

/**
* @brief  Destroy fifo object
* @param[in] fifo : fifo object 
**/
void DestroyDgFifo(dg_fifo *fifo);

/**
* @brief  Destroy fifo object
* @param[in] fifo     : fifo object
* @param[in] data     : write data to fifo
* @param[in] dataSize : data size
* @return  fifo current size if OK, -1 on error
**/
int WriteDgFifo(dg_fifo *fifo, const void *data, int dataSize);

/**
* @brief  Destroy fifo object
* @param[in]  fifo     : fifo object
* @param[out] data     : read data from fifo
* @param[out] dataSize : data size
* @return  fifo current size if OK, -1 on error
**/
int ReadDgFifo(dg_fifo *fifo, void *data, int *dataSize);

/**
* @brief  Get fifo empty status
* @param[in] fifo : fifo object
* @return  true if Empty, false not Empty
**/
bool DgFifoEmpty(dg_fifo *fifo);

/**
* @brief  Get fifo full status
* @param[in] fifo : fifo object
* @return  true if Full, false not Full
**/
bool DgFifoFull(dg_fifo *fifo);

/*
* @brief Define sliding window struct
*/
typedef struct dg_sliding_wnd_t
{
    int  base;          // base window index
    int  top;           // top window index
    int  next;          // expected data index
    int  size;          // sliding window size
    int  win;           // remain window size
}dg_sliding_wnd;

/*
* @brief Define receive buffer struct
*/
typedef struct dg_rcv_buf_t
{
    uint32_t        frameSize;      // buffer frame size
    uint32_t        firstSeq;       // first seq number
    uint32_t        nextSeq;        // expected seq number
    uint32_t        ts;             // ack's timestamp
    pthread_mutex_t	mutex;          // mutex value
    dg_sliding_wnd  rwnd;           // receive sliding window
    struct filedatagram *buffer;    // buffer array, the size is frameSize
}dg_rcv_buf;

/**
* @brief  Create receive buffer object
* @param[in] wndSize  : sliding window size
* @return  receive buffer object if OK, NULL on error
**/
dg_rcv_buf *CreateDgRcvBuf(int wndSize);

/**
* @brief  Destroy receive buffer object
* @param[in] buf : receive buffer object
**/
void DestroyDgRcvBuf(dg_rcv_buf *buf);

/**
* @brief  Write data to receive buffer
* @param[in] buf  : receive buffer object
* @param[in] data : struct filedatagram data
* @return 0 if OK, -1 on error
**/
int WriteDgRcvBuf(dg_rcv_buf *buf, const struct filedatagram *data);

/**
* @brief  Read data from receive buffer object
* @param[in] buf  : receive buffer object
* @param[in] data : struct filedatagram data
* @param[in] need : if 1 read current data
* @return if more than one segments data to read, return the segments number, -1 on error
**/
int ReadDgRcvBuf(dg_rcv_buf *buf, struct filedatagram *data, int need);

#endif // __DG_BUFFER_H_

