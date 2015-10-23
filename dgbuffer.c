/**
* @file         :  dgbuffer.c
* @author       :  Jiewen Zheng
* @date         :  2015-10-13
* @brief        :  Receive data buffer and FIFO implementation
* @changelog    :
**/


#include <stddef.h>

#include "dgbuffer.h"

/****************************************
*
* @brief FIFO implementation
*
*****************************************/

dg_fifo *CreateDgFifo(int size)
{
    dg_fifo *fifo = malloc(sizeof(dg_fifo));
    fifo->size = size;
    fifo->curSize = 0;
    fifo->head = NULL;
    fifo->curData = NULL;

    // initial mutex
    Pthread_mutex_init(&fifo->mutex, NULL);

    return fifo;
}

void DestroyDgFifo(dg_fifo *fifo)
{
    dg_node *node = fifo->head;
    for (; node != NULL; node = node->next)
    {
        if (node->data)
            free(node->data);
    }

    if (fifo)
    {
        free(fifo);
        fifo = NULL;
    }

    // destroy mutex
    pthread_mutex_destroy(&fifo->mutex);
}

int WriteDgFifo(dg_fifo *fifo, const void *data, int dataSize)
{
    if (fifo->curSize == fifo->size)
    {
        // fifo is full
        return -1;
    }

    // lock
    Pthread_mutex_lock(&fifo->mutex);

    // create node data
    dg_node *node = malloc(sizeof(dg_node));
    node->data = malloc(dataSize);
    memcpy(node->data, data, dataSize);
    node->size = dataSize;

    if (fifo->curSize == 0)
    {
        fifo->head = node;
        fifo->curData = fifo->head;
        fifo->head->next = NULL;
        fifo->curData->next = NULL;
    }
    else
    {
        fifo->curData->next = node;    
        fifo->curData = fifo->curData->next;
        fifo->curData->next = NULL;
    }

    fifo->curSize++;
    
    // unlock
    Pthread_mutex_unlock(&fifo->mutex);

    return fifo->curSize;
}

int ReadDgFifo(dg_fifo *fifo, void *data, int *dataSize)
{
    if (fifo->curSize == 0 ||   // fifo is empty
        data == NULL)           // data is invalid
    {   
        return -1;
    }        

    // lock
    Pthread_mutex_lock(&fifo->mutex);

    dg_node *node = fifo->head;

    fifo->head = fifo->head->next;

    memcpy(data, node->data, node->size);
    *dataSize = node->size;

    free(node->data);
    free(node);

    fifo->curSize--;

    // unlock
    Pthread_mutex_unlock(&fifo->mutex);
    
    return fifo->curSize;
}

bool DgFifoEmpty(dg_fifo *fifo)
{
    if (fifo == NULL)
        return false;

    return (fifo->curSize == fifo->size);
}

bool DgFifoFull(dg_fifo *fifo)
{
    if (fifo == NULL)
        return false;

    return (fifo->curSize == 0);
}


/****************************************
*
* @brief Receive buffer implementation
*
*****************************************/

dg_rcv_buf *CreateDgRcvBuf(int wndSize)
{
    dg_rcv_buf *rcvBuf = malloc(sizeof(dg_rcv_buf));
    rcvBuf->frameSize = 2 * wndSize;
    rcvBuf->firstSeq = 0;
    int size = rcvBuf->frameSize * sizeof(struct filedatagram);
    rcvBuf->buffer = malloc(size);
    memset(rcvBuf->buffer, 0, size);
    
    // initial sliding window index
    rcvBuf->rwnd.base = 0;
    rcvBuf->rwnd.next = 0;
    rcvBuf->rwnd.top = wndSize;
    rcvBuf->rwnd.size = wndSize;
    rcvBuf->rwnd.win = wndSize;

    return rcvBuf;
}

void DestroyDgRcvBuf(dg_rcv_buf *buf)
{
    if (buf && buf->buffer)
    {
        free(buf->buffer);
        buf->buffer = NULL;
    }

    if (buf)
    {
        free(buf);
        buf = NULL;
    }
}

// check current seq number in sliding window
int CheckSeqRange(dg_rcv_buf *buf, int idx)
{
    int r = buf->rwnd.top;
    int n = buf->rwnd.next;
    if (buf->rwnd.base >= buf->rwnd.top)
        r = buf->rwnd.top + buf->frameSize;
    if (buf->rwnd.base > buf->rwnd.next)
        n = buf->rwnd.next + buf->frameSize;

    if (idx > r ||  // out of window size
        idx < n)    // less than expect segment seq
    {
        // out of range, can't save in buffer
        printf("Window index=%d out of range[%d, %d]\n", idx, n, r);
        return -1;
    }

    return 0;
}

int WriteDgRcvBuf(dg_rcv_buf *buf, const struct filedatagram *data)
{
    // check sliding window 
    if (buf->rwnd.win == 0)
    {
        // sliding window is full
        printf("[Client]: Receive sliding window is full\n");
        return -1;
    }

    int ack = 0;
    int idx = data->seq % buf->frameSize;

    if (buf->buffer[idx].seq == data->seq)
    {
        printf("[Client]: Receive seq=%d, already in buffer\n", data->seq);
        return -1;
    }

    dg_sliding_wnd *rwnd = &buf->rwnd;
    if (buf->firstSeq == 0)
    {
        // first seq, initial sliding window
        buf->firstSeq = data->seq;
        rwnd->base = idx;
        rwnd->top = rwnd->base + rwnd->size;
        rwnd->next = (idx + 1) % buf->frameSize;
        buf->nextSeq = data->seq + 1;
        buf->ts = data->ts;
    }
    else
    {
        if (CheckSeqRange(buf, idx) < 0)
        {
            printf("[Client]: Receive seq=%d out of range, win[%d, %d] next=%d win=%d\n",
                data->seq, buf->rwnd.base, buf->rwnd.top, buf->rwnd.next, buf->rwnd.win);
            return -1;
        }

        if (idx == rwnd->next)
        {
            do
            {
                // in-order
                // expected seq number
                buf->nextSeq++;
                buf->ts = data->ts;
                rwnd->next = (buf->rwnd.next + 1) % buf->frameSize;

            } while(buf->buffer[rwnd->next].seq != 0);
        }
        else
        {
            // out-of-order segment
            // send duplicate ACK, indicating seq of next expected
            ack = buf->nextSeq;
        }
    }

    memcpy(&buf->buffer[idx], data, sizeof(struct filedatagram));
    rwnd->win--;

    printf("[Client]: Receive seq=%d ts=%d, win[%d, %d] next=%d win=%d\n",
        data->seq, data->ts, buf->rwnd.base, buf->rwnd.top, buf->rwnd.next, buf->rwnd.win);
    
    return ack;
}

int ReadDgRcvBuf(dg_rcv_buf *buf, struct filedatagram *data, int need)
{
    int flag = 0;
    int inOrderPkt = 0;
    if (buf->rwnd.next < buf->rwnd.base)
        inOrderPkt = (buf->rwnd.next + buf->frameSize) - buf->rwnd.base;
    else
        inOrderPkt = buf->rwnd.next - buf->rwnd.base;

    if (need == 1)
    {
        if (inOrderPkt > 0)
            flag = 1;
    }
    else
    {
        int buffered = buf->rwnd.size - buf->rwnd.win;
        if (inOrderPkt < buffered)
        {
            // there are segment gaps, waiting to some segments fill the gaps
            return -1;
        }
        else
        {
            if (inOrderPkt > 1) // more than 2 in-order segments
                flag = 1;
        }
    }

    if (flag)
    {
        int idx = buf->rwnd.base;
        memcpy(data, &buf->buffer[idx], sizeof(struct filedatagram));
        memset(&buf->buffer[idx], 0, sizeof(struct filedatagram));        

        // slide window to right
        buf->rwnd.base = (buf->rwnd.base + 1) % buf->frameSize;
        buf->rwnd.top = (buf->rwnd.top + 1) % buf->frameSize;
        buf->rwnd.win++;

        printf("put  seq=%d to app, win[%d, %d] next=%d win=%d\n", 
            data->seq, buf->rwnd.base, buf->rwnd.top, buf->rwnd.next, buf->rwnd.win);

        return --inOrderPkt;
    }
    
    return -1;    
}



