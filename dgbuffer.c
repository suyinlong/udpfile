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
    if (fifo == NULL)
        return;

    // destroy mutex
    pthread_mutex_destroy(&fifo->mutex);

    // free the list nodes
    dg_node *node = fifo->head;
    for (; node != NULL; node = node->next)
    {
        if (node->data)
            free(node->data);
    }

    // free dg_fifo object
    free(fifo);
    fifo = NULL;
}

void DgLock(pthread_mutex_t mutex)
{
    // lock
    Pthread_mutex_lock(&mutex);
}

void DgUnlock(pthread_mutex_t mutex)
{
    Pthread_mutex_unlock(&mutex);
}

int WriteDgFifo(dg_fifo *fifo, const void *data, int dataSize)
{
    if (fifo->curSize == fifo->size)
    {
        // fifo is full
        return -1;
    }

    // lock
    DgLock(fifo->mutex);

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
    DgUnlock(fifo->mutex);

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
    DgLock(fifo->mutex);

    dg_node *node = fifo->head;

    fifo->head = fifo->head->next;

    // copy data
    memcpy(data, node->data, node->size);
    *dataSize = node->size;

    free(node->data);
    free(node);

    fifo->curSize--;

    // unlock
    DgUnlock(fifo->mutex);

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
    dg_rcv_buf *buf = malloc(sizeof(dg_rcv_buf));
    buf->frameSize = 2 * wndSize;
    buf->firstSeq = 0;
    int size = buf->frameSize * sizeof(struct filedatagram);
    buf->buffer = malloc(size);
    memset(buf->buffer, 0, size);

    // initial sliding window index
    buf->rwnd.base = 0;
    buf->rwnd.next = 0;
    buf->rwnd.top = wndSize;
    buf->rwnd.size = wndSize;
    buf->rwnd.win = wndSize;

    // initial mutex
    Pthread_mutex_init(&buf->mutex, NULL);

    return buf;
}

void DestroyDgRcvBuf(dg_rcv_buf *buf)
{
    if (buf == NULL)
        return;

    // destroy mutex
    pthread_mutex_destroy(&buf->mutex);

    if (buf->buffer)
    {
        free(buf->buffer);
        buf->buffer = NULL;
    }

    // free dg_rcv_buf object
    free(buf);
    buf = NULL;
}

// check current seq number in sliding window
int CheckSeqRange(dg_rcv_buf *buf, uint32_t idx)
{
    int top = buf->rwnd.top;
    int next = buf->rwnd.next;

    if (top > next)
    {
        if (idx > top ||  // out of window size
            idx < next)    // less than expect segment seq
            return -1;
    }
    else
    {
        if (idx < next)
            return -1;
    }

    /*
    if (buf->rwnd.base >= buf->rwnd.top)
        top = buf->rwnd.top + buf->frameSize;
    if (buf->rwnd.base > buf->rwnd.next)
        next = buf->rwnd.next + buf->frameSize;
    if (idx > top ||  // out of window size
        idx < next)    // less than expect segment seq
    {
        // out of range, can't save in buffer
        printf("[Client]: Window index=%d out of range[%d, %d]\n", idx, next, top);
        return -1;
    }
    */

    return 0;
}

int WriteDgRcvBuf(dg_rcv_buf *buf, const struct filedatagram *data, int print, uint32_t *ack)
{
    // check sliding window
    if (buf->rwnd.win == 0)
    {
        // sliding window is full
        printf("[Client]: Receive datagram #%d error: sliding window is full\n", data->seq);
        return DGBUF_RWND_FULL;
    }

    int ret = 0;
    uint32_t idx = data->seq % buf->frameSize;

    if (buf->firstSeq > 0 && buf->buffer[idx].seq == data->seq)
    {
        if (print)
            printf("[Client]: Receive datagram #%d, seq=%d, is already in buffer\n", data->seq, data->seq);
        return DGBUF_SEGMENT_IN_BUF;
    }

    // lock
    DgLock(buf->mutex);

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
            if (print)
                printf("[Client]: Receive datagram #%d, seq=%d idx=%d, is out of range, rwin[%d, %d] next=%d win=%d\n",
                    data->seq, data->seq, idx, buf->rwnd.base, buf->rwnd.top, buf->rwnd.next, buf->rwnd.win);
            DgUnlock(buf->mutex);
            return DGBUF_SEGMENT_OUTOFRANGE;
        }

        if (idx == rwnd->next)
        {
            do
            {
                // in-order
                // expected seq number
                buf->nextSeq++;
                buf->ts = data->ts;   // store current timestamp
                rwnd->next = (buf->rwnd.next + 1) % buf->frameSize;

            } while(buf->buffer[rwnd->next].seq != 0);  // check next frame
        }
        else
        {
            // out-of-order segment
            // send duplicate ACK, indicating seq of next expected
            *ack = buf->nextSeq;
            ret = DGBUF_SEGMENT_OUTOFORDER;
        }
    }

    memcpy(&buf->buffer[idx], data, sizeof(struct filedatagram));
    rwnd->win--;

    if (print)
    {
        printf("[Client]: Receive datagram #%d [seq=%d ts=%d] flag[eof=%d pob=%d] rwnd=%d\n",
            data->seq, data->seq, data->ts,
            data->flag.eof, data->flag.pob, rwnd->win);
    }

    // unlock
    DgUnlock(buf->mutex);

    return ret;
}

int ReadDgRcvBuf(dg_rcv_buf *buf, struct filedatagram *data, int need)
{
    int flag = 0;
    int inOrderPkt = 0;

    // lock
    DgLock(buf->mutex);

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
            DgUnlock(buf->mutex);
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

#ifdef DEBUG_BUFFER
        printf("[RcvBuf]: Read buffer, seq=%d ts=%d win=%d\n", data->seq, data->ts, buf->rwnd.win);
#endif

        // unlock
        DgUnlock(buf->mutex);
        return --inOrderPkt;
    }

    // unlock
    DgUnlock(buf->mutex);

    return -1;
}


int GetInOrderAck(dg_rcv_buf *buf, uint32_t *ack, uint32_t *ts)
{
    int inOrderPkt = 0;

    // lock
    DgLock(buf->mutex);

    if (buf->rwnd.next < buf->rwnd.base)
        inOrderPkt = (buf->rwnd.next + buf->frameSize) - buf->rwnd.base;
    else
        inOrderPkt = buf->rwnd.next - buf->rwnd.base;

    *ack = 0;
    *ts = 0;
    if (inOrderPkt > 1)
    {
        // get last received segment
        int idx = buf->rwnd.next - 1;
        if (buf->rwnd.next == 0)
            idx = buf->frameSize - 1;

        // last received segment seq - acked seq >= 2,
        // then send a ack to server
        if (buf->buffer[idx].seq - buf->acked < 1)
        {
            DgUnlock(buf->mutex);
            return -1;
        }

        *ack = buf->buffer[idx].seq+1;
        *ts = buf->buffer[idx].ts;

        DgUnlock(buf->mutex);
        return 0;
    }

    // unlock
    DgUnlock(buf->mutex);

    return -1;
}


