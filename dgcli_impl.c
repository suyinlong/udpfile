/**
* @file         :  dgcli_impl.c
* @author       :  Jiewen Zheng
* @date         :  2015-10-13
* @brief        :  UDP datagram client implementation
* @changelog    :
**/

#include <math.h>
#include <setjmp.h>

#include "dgcli_impl.h"

#define RCV_TIMEOUT      5           // 5 seconds
#define SIG_DELAYEDACK  (SIGRTMAX)
#define RANDOM_MAX       0x7FFFFFFF  // 2^31 - 1
#define FIN_TIMEWAIT     30          // 30 seconds

sigjmp_buf g_jmpbuf;
int        g_threadStop;

void   GetDatagram(dg_client *cli, int need);
double DgRandom();
void   SetRTTTimer(uint32_t timeout);

dg_client *CreateDgCli(const dg_arg *arg, int sock)
{
    dg_client *cli = malloc(sizeof(dg_client));
    cli->arg = (dg_arg *)arg;
    cli->sock = sock;
    cli->timeout = RCV_TIMEOUT;
    cli->seq = 0;
    cli->printSeq = 1;
    cli->printFile = 1;

    // create a receive buffer, the buffer size is
    // twice the receive sliding window size
    cli->buf = CreateDgRcvBuf(cli->arg->rcvWin);

    // create a fifo, FIFO_SIZE is 512
    cli->fifo = CreateDgFifo(FIFO_SIZE);

    return cli;
}

void DestroyDgCli(dg_client *cli)
{
    if (cli == NULL)
        return;

    // destroy the receive buffer
    DestroyDgRcvBuf(cli->buf);

    // destroy the fifo
    DestroyDgFifo(cli->fifo);

    // free dg_client object resource
    free(cli);
    cli = NULL;
}

// handle connect server time out
void HandleConnectTimeout(int signo)
{
    siglongjmp(g_jmpbuf, 1);
}

// handle fin time out
void HandleFinTimeout(int signo)
{
    siglongjmp(g_jmpbuf, 1);
}

// handle receive data time out
void HandleRecvTimeout(int signo)
{
    // just interrupt the operation
}

// handle delayed ack time out
void HandleDelayedAckTimeout(int signo, siginfo_t *siginfo, void *context)
{
    dg_client *cli;
    cli = (dg_client *)siginfo->si_value.sival_ptr;
    if (cli == NULL)
        return;

    int need = 1;
    GetDatagram(cli, need);
}

// reconnect the server
void ReconnectDgSrv(dg_client *cli)
{
    struct sockaddr_in srvAddr;

    // reconnect
    bzero(&srvAddr, sizeof(srvAddr));
    srvAddr.sin_family = AF_INET;
    srvAddr.sin_port = htons(cli->newPort);
    Inet_pton(AF_INET, cli->arg->srvIP, &srvAddr.sin_addr);

    Connect(cli->sock, (SA *)&srvAddr, sizeof(srvAddr));

    printf("[Client]: Reconnect server %s:%d\n", cli->arg->srvIP, cli->newPort);
}

// receive data with timer
int RecvDataTimeout(dg_client *cli, void *data, int *size)
{
    int	ret = 0;
    int retry = RTT_MAXNREXMT;

#if 0
    rtt_init(&cli->rtt);
    rtt_newpack(&cli->rtt);
#endif

read_data_again:
    ret = Dg_readpacket(cli->sock, data);
    if (ret == -1 && (errno == EINTR || errno == ECONNREFUSED))
    {
#if 0
        usleep(3 * rtt_start(&cli->rtt) * 1000);        
        if (rtt_timeout(&cli->rtt) < 0)
        {
            rtt_stop(&cli->rtt, rtt_ts(&cli->rtt));
            err_msg("[Client]: Read datagram from socket error, giving up\n");
            return -1;
        }
#endif

        goto read_data_again;
    }

    *size = ret;

    if (cli->arg->p > 0 && DgRandom() <= cli->arg->p)
        // discard the datagram
        goto read_data_again;
    
    return ret;
}

// send a filename request to server
int SendDgSrvFilenameReq(dg_client *cli)
{
    struct filedatagram sndData, rcvData;
    // init filedatagram
    bzero(&sndData, sizeof(sndData));
    sndData.seq = cli->seq;
    sndData.ts = rtt_ts(&cli->rtt);
    sndData.wnd = cli->buf->rwnd.size;
    sndData.flag.fln = 1;
    sndData.len = strlen(cli->arg->filename);
    strcpy(sndData.data, cli->arg->filename);

    // calc timeout value & start timer
    SetRTTTimer(rtt_start(&cli->rtt));

    if (sigsetjmp(g_jmpbuf, 1) != 0)
    {
        if (rtt_timeout(&cli->rtt) < 0)
        {
            errno = 0;
            err_msg("[Client]: No response from server %s:%d, giving up", cli->arg->srvIP, cli->arg->srvPort);
        }
        else
        {
            errno = ETIMEDOUT;  // timeout, retransmitting
        }
        return -1;
    }

    // if random() <= p, discard the datagram (just don't send)
    if (DgRandom() > cli->arg->p)
        Dg_writepacket(cli->sock, &sndData);

read_port_reply_again:

    bzero(&rcvData, sizeof(rcvData));
    Dg_readpacket(cli->sock, &rcvData);
    if (cli->arg->p > 0 && DgRandom() <= cli->arg->p)
    {
        // discard the datagram
        goto read_port_reply_again;
    }

    // stop rtt timer
    SetRTTTimer(0);
    // calculate & store new RTT estimator values
    rtt_stop(&cli->rtt, rtt_ts(&cli->rtt) - rcvData.ts);

    if (rcvData.flag.pot != 1)
    {
        printf("[Client]: Received an invalid packet (no port number).\n");
        return -1;
    }
    else
    {
        cli->newPort = atoi(rcvData.data);
        printf("[Client]: Received a valid private port number %d from server.\n", cli->newPort);
    }

    return 0;
}

// send new port ack
int SendDgSrvNewPortAck(dg_client *cli, struct filedatagram *data)
{
    struct filedatagram sndData;
    // init filedatagram
    bzero(&sndData, sizeof(sndData));

    sndData.seq = cli->seq++;
    sndData.ack = 1;
    sndData.wnd = cli->buf->rwnd.size;
    sndData.flag.pot = 1;
    sndData.len = 0;

    bzero(data, sizeof(*data));
    while (1)
    {
        // if random() <= p, discard the datagram (just don't send)
        if (DgRandom() > cli->arg->p)
            Dg_writepacket(cli->sock, &sndData);

    read_port_again:
        Dg_readpacket(cli->sock, data);
        if (cli->arg->p > 0 && DgRandom() <= cli->arg->p)
        {
            // discard the datagram
            goto read_port_again;
        }

        if (data->seq > 0)
            break;
    }

    return 0;
}

// send ack to server
void SendDgSrvAck(dg_client *cli, uint32_t ack, uint32_t ts, int wnd, int wndFlag, const char *tag)
{
    struct filedatagram dg;
    // init filedatagram
    bzero(&dg, sizeof(dg));

    dg.seq = cli->seq++;
    dg.ack = ack;
    dg.ts = ts;
    dg.flag.wnd = wndFlag;
    dg.wnd = wnd;
    dg.len = 0;
    cli->buf->acked = ack;

    if (DgRandom() > cli->arg->p)
        Dg_writepacket(cli->sock, &dg);

    if (cli->printSeq)
        printf("[Client #%d]: Send ACK #%d [ack=%d seq=%d ts=%d win=%d flag.wnd=%d] (%s)\n",
            pthread_self(), dg.ack, dg.ack, dg.seq, dg.ts, dg.wnd, dg.flag.wnd, tag);
}

// handle client to finish work
void HandleDgClientFin(dg_client *cli)
{
    Signal(SIGALRM, HandleFinTimeout);

    // start fin timer
    alarm(FIN_TIMEWAIT);

    dg_client *dc = cli;
    if (sigsetjmp(g_jmpbuf, 1) != 0)
    {
        if (g_threadStop == 1)
        {
            // timeout, retransmitting
            printf("[Client]: Application exited\n");
            alarm(0);
            exit(0);
        }
        else
        {
            // reset
            HandleDgClientFin(dc);
        }
    }

    if (cli->printSeq)
        printf("[Client]: Wait timer(%ds) to clean close\n", FIN_TIMEWAIT);
}

//  print file content thread
void *PrintOutThread(void *arg)
{
    if (arg == NULL)
    {
        printf("[Client]: Print out work thread error.\n");
        return;
    }

    dg_client *cli = (dg_client *)arg;

    int size = 0;
    int ret = 0;
    int d = 0;
    struct filedatagram fd;

    g_threadStop = 0;
    printf("[Client]: Print thread #%d is working\n", pthread_self());

    while (1)
    {
        // get data from fifo
        ret = ReadDgFifo(cli->fifo, &fd, &size);
        if (ret < 0)
        {
            // produce a random double in the range (0.0, 1.0)
            d = -1 * cli->arg->u * log(DgRandom()) * 1000;
            usleep(d);
            continue;
        }

        //printf("[Thread #%d]: Read fifo, seq=%d ack=%d ts=%d wnd=%d flag.eof=%d len=%d" \
            "\n--------------------\n%s\n--------------------\n", \
            pthread_self(), fd.seq, fd.ack, fd.ts, fd.wnd, fd.flag.eof, fd.len, fd.data);

        if (cli->printFile)
            printf("%s", fd.data);

        if (fd.flag.eof == 1)
        {
            printf("[Thread #%d]: File data finished\n", pthread_self());
            break;
        }
    }

    g_threadStop = 1;
    printf("[Client]: Print thread #%d exited\n", pthread_self());
}

// create a thread
void CreateThread(dg_client *cli)
{
    pthread_t tid;
    Pthread_create(&tid, NULL, &PrintOutThread, cli);

    printf("[Client]: Create thread ok, tid=%d\n", tid);
}

// set rtt timer
void SetRTTTimer(uint32_t timeout)
{
    struct itimerval it;
    it.it_value.tv_sec = timeout / 1000;
    it.it_value.tv_usec = (timeout % 1000) * 1000;
    it.it_interval.tv_sec = 0;
    it.it_interval.tv_usec = 0;

    setitimer(ITIMER_REAL, &it, NULL);
}

// set delayed ack timer
int SetDelayedAckTimer(dg_client *cli)
{
    struct sigaction act;

    sigemptyset(&act.sa_mask);
    act.sa_sigaction = HandleDelayedAckTimeout; // handle dealyed ack function
    act.sa_flags = SA_SIGINFO;

    // register the SIG_DELAYEDACK signal
    if (sigaction(SIG_DELAYEDACK, &act, NULL) < 0)
    {
        printf("[Client]: Sigaction error\n");
        return -1;
    }

    struct sigevent evp;
    evp.sigev_notify = SIGEV_SIGNAL;
    evp.sigev_signo = SIG_DELAYEDACK;
    evp.sigev_value.sival_ptr = cli;    // pass the client object

    // create a timer
    if (timer_create(CLOCK_REALTIME, &evp, &cli->delayedAckTimer) < 0)
    {
        printf("[Client]: timer_create error\n");
        return -1;
    }

    // set 500ms delayed ack timer
    int n = 500;
    struct itimerspec it;
    it.it_interval.tv_sec = 0;
    it.it_interval.tv_nsec = n * 1000000;
    it.it_value.tv_sec = 0;
    it.it_value.tv_nsec = 500 * 1000000;

    // set the timer
    if (timer_settime(cli->delayedAckTimer, 0, &it, NULL))
    {
        printf("[Client]: timer_settime error\n");
        return -1;
    }

    return 0;
}

// connect server with RTO
int ConnectDgServer(dg_client *cli)
{
    int ret = 0;
    struct filedatagram dg;

    Signal(SIGALRM, HandleConnectTimeout);

    // initialize rtt
    rtt_init(&cli->rtt);
    rtt_newpack(&cli->rtt);

    do
    {
        // connect server
        ret = SendDgSrvFilenameReq(cli);
        if (ret < 0)
        {
            if (errno == ETIMEDOUT)
            {
                printf("[Client]: Send filename to server %s:%d error #%d: %s\n", cli->arg->srvIP, cli->arg->srvPort, cli->rtt.rtt_nrexmt, strerror(errno));
                continue;
            }

            printf("[Client]: Connect server %s:%d error\n", cli->arg->srvIP, cli->arg->srvPort);
            return -1;
        }

        // reconnect server with new port number
        ReconnectDgSrv(cli);

        // send port ack
        ret = SendDgSrvNewPortAck(cli, &dg);

    } while (ret < 0);

    printf("[Client]: Connect server %s:%d ok\n", cli->arg->srvIP, cli->newPort);

    uint32_t ack = 0;
    // save first segment
    WriteDgRcvBuf(cli->buf, &dg, cli->printSeq, &ack);

    return 0;
}

// get datagram from buffer
// 1. check the receiver window
// 2. if there is in-order segments, put those segments to fifo
// 3. if there is more than 2 in-order segments, send ack to server
void GetDatagram(dg_client *cli, int need)
{
    int ret = 0, old_win = cli->buf->rwnd.win;
    struct filedatagram dg;

    do
    {
        // get data from receive buffer
        ret = ReadDgRcvBuf(cli->buf, &dg, need);
        if (ret != -1)
        {
            // put data to fifo
            WriteDgFifo(cli->fifo, &dg, sizeof(dg));

            //printf("[Client #%d]: Write datagram to fifo, seq=%d ack=%d ts=%d len=%d\n", \
                pthread_self(), dg.seq, dg.ack, dg.ts, dg.len);
        }
        need = ret;
    } while (ret > 0);

    if (ret != -1)
    {
        // segments in-order, send ack to server
        if (old_win == 0)
            SendDgSrvAck(cli, dg.seq + 1, 0/*dg.ts*/, cli->buf->rwnd.win, 1, "in-order & update rwnd");
        else
            SendDgSrvAck(cli, dg.seq + 1, 0/*dg.ts*/, cli->buf->rwnd.win, 0, "in-order");
    }
}

double DgRandom()
{
    // produce a random double in the range [0.0, 1.0]
    double r = random() / (double)RANDOM_MAX;
    return r;
}

int StartDgCli(dg_client *cli)
{
    if (NULL == cli)
        return -1;

    int ret = 0;

    // initialize random
    srandom(cli->arg->seed);

    // connect server
    if (ConnectDgServer(cli) < 0)
        return -1;

    // create print out thread
    CreateThread(cli);

    // set and start delayed ack timer
    if (SetDelayedAckTimer(cli))
        return -1;

    struct filedatagram dg = { 0 };
    int sz = sizeof(dg);

    // main loop
    while (1)
    {
        bzero(&dg, sizeof(dg));
        // receive data
        ret = RecvDataTimeout(cli, &dg, &sz);
        if (ret < 0)
        {
            if (errno == ETIMEDOUT || errno == EAGAIN)
                continue;
            else
                break;
        }

#if 0
        dg.len = ntohs(dg.len);
        dg.seq = ntohl(dg.seq);
        dg.ack = ntohl(dg.ack);
        dg.ts  = ntohl(dg.ts);
        dg.wnd = ntohs(dg.wnd);
#endif


        //printf("dg.seq=%d ret=%d, rwnd.base=%d rwnd.next=%d rwnd.top=%d\n", \
            dg.seq, ret, cli->buf->rwnd.base, cli->buf->rwnd.next, cli->buf->rwnd.top);

        // received window probe
        if (dg.flag.pob == 1)
        {
            // send current window size
            SendDgSrvAck(cli, cli->buf->nextSeq, dg.ts, cli->buf->rwnd.win, 1, "received window probe");
            continue;
        }

        // received eof
        if (dg.flag.eof == 1)
        {
            HandleDgClientFin(cli);
        }

        int ret = 0;
        uint32_t ack = 0, ts = 0;
        // put data to receive buffer
        ret = WriteDgRcvBuf(cli->buf, &dg, cli->printSeq, &ack);
        switch (ret)
        {
        case DGBUF_RWND_FULL:   // sliding window size is zero
            SendDgSrvAck(cli, cli->buf->nextSeq, dg.ts, cli->buf->rwnd.win, 1, "rwnd size is 0");
            continue;

        case DGBUF_SEGMENT_IN_BUF:      // segment is already in receive buffer
            SendDgSrvAck(cli, cli->buf->nextSeq, dg.ts, cli->buf->rwnd.win, 0, "already-in buffer");
            continue;
        
        case DGBUF_SEGMENT_OUTOFRANGE:  // segment is out of range
            SendDgSrvAck(cli, cli->buf->nextSeq, dg.ts, cli->buf->rwnd.win, 0, "out-of-range");
            continue;

        case DGBUF_SEGMENT_OUTOFORDER:  // out of order, send duplicate ack
            SendDgSrvAck(cli, ack, cli->buf->ts, cli->buf->rwnd.win, 0, "out-of-order");
            //break;
            continue;
        }

        // get datagram from buffer
        if (GetInOrderAck(cli->buf, &ack, &ts) == 0)
        {
            SendDgSrvAck(cli, ack, ts, cli->buf->rwnd.win, 0, "in-order");
        }
    }

    return 0;
}
