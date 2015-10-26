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

sigjmp_buf g_jmpbuf;

//void   SendDgSrvAck(dg_client *cli, uint32_t ack, uint32_t ts, int wnd, int line);
void   GetDatagram(dg_client *cli, int need);
double DgRandom();

dg_client *CreateDgCli(const dg_arg *arg, int sock)
{
    dg_client *cli = malloc(sizeof(dg_client));
    cli->arg = (dg_arg *)arg;
    cli->sock = sock;
    cli->timeout = RCV_TIMEOUT;
    cli->seq = 0;

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
int RecvDataTimeout(int fd, void *data, int *size, int timeout, float p)
{
    Sigfunc *sigfunc;
    // set alarm
    sigfunc = Signal(SIGALRM, HandleRecvTimeout);
    alarm(timeout);
    /*
    if (alarm(timeout) != 0)
    {
        printf("[Client]: Alarm was already set.\n");
        return -1;
    }
    */

    int	ret = 0;
    // read data 
    if ((ret = read(fd, data, *size)) < 0)
    {
        if (errno == EINTR)
            errno = ETIMEDOUT;
    }

    if (p > 0 && DgRandom() <= p)
    {
        // discard the datagram
        errno = EAGAIN;
        ret = -1;
    }
    

    // turn off the alarm
    alarm(0);
    // restore previous signal handler
    Signal(SIGALRM, sigfunc);

    return ret;
}

// send a filename request to server 
int SendDgSrvFilenameReq(dg_client *cli)
{
    struct filedatagram sndData, rcvData;
    // init filedatagram
    bzero(&sndData, sizeof(sndData));
    bzero(&rcvData, sizeof(rcvData));

    sndData.seq = cli->seq;
    sndData.ts = rtt_ts(&cli->rtt);
    sndData.flag.fln = 1;
    sndData.len = strlen(cli->arg->filename);
    strcpy(sndData.data, cli->arg->filename);

    // calc timeout value & start timer
    alarm(rtt_start(&cli->rtt));

    if (sigsetjmp(g_jmpbuf, 1) != 0)
    {
        if (rtt_timeout(&cli->rtt) < 0)        
            err_msg("[Client]: No response from server %s:%d, giving up", cli->arg->srvIP, cli->arg->srvPort);
        else
            errno = ETIMEDOUT;  // timeout, retransmitting       
        alarm(0);
        return -1;  
    }

    //while (1)
    {
        // if random() <= p, discard the datagram (just don't send)
        if (DgRandom() > cli->arg->p)
            Dg_writepacket(cli->sock, &sndData);

        Dg_readpacket(cli->sock, &rcvData);
        if (cli->arg->p > 0 && DgRandom() <= cli->arg->p)
        {
            // discard the datagram
            errno = EAGAIN;
            alarm(0);
            return -1;
        }
        /*
        // wait for server data, use a timer
        int rcvSize = sizeof(rcvData);
        if (RecvDataTimeout(cli->sock, &rcvData, &rcvSize, cli->timeout, cli->arg->p) < 0)
        {
            if (errno == EAGAIN)                
                return 0;

            if (errno != ETIMEDOUT)
                return -1; // error
        }
        else
        {
            // successfully receive data from server
            break;
        }
        */
    }
    
    // stop SIGALRM timer
    alarm(0);
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
    struct filedatagram sndData, rcvData;
    // init filedatagram
    bzero(&sndData, sizeof(sndData));
    bzero(&rcvData, sizeof(rcvData));
    sndData.seq = cli->seq++;
    sndData.ack = 1;
    sndData.wnd = cli->buf->rwnd.size;
    sndData.flag.pot = 1;
    sndData.len = 0;

    // calc timeout value & start timer
    alarm(rtt_start(&cli->rtt));

    if (sigsetjmp(g_jmpbuf, 1) != 0)
    {
        if (rtt_timeout(&cli->rtt) < 0)
            err_msg("[Client]: Send port ack, no response from server %s:%d, giving up", cli->arg->srvIP, cli->newPort);
        else
            errno = ETIMEDOUT;  // timeout, retransmitting
        alarm(0);
        return -1;
    }

    // if random() <= p, discard the datagram (just don't send)
    if (DgRandom() > cli->arg->p)
        Dg_writepacket(cli->sock, &sndData);

    Dg_readpacket(cli->sock, &rcvData);
    if (cli->arg->p > 0 && DgRandom() <= cli->arg->p)
    {
        // discard the datagram
        errno = EAGAIN;
        alarm(0);
        return -1;
    }

    /*
    while (1)
    {
    // if random() <= p, discard the datagram (just don't send)
    if (DgRandom() > cli->arg->p)
    Dg_writepacket(cli->sock, &dg);

    // wait for server data, use a timer
    int rcvSize = sizeof(struct filedatagram);
    if (RecvDataTimeout(cli->sock, data, &rcvSize, cli->timeout, cli->arg->p) < 0)
    {
    if (errno == EAGAIN)
    return 0;

    if (errno != ETIMEDOUT)
    return -1; // error
    }
    else
    {
    // successfully receive data from server
    break;
    }
    }
    */

    return 0;
}

// send ack to server
void SendDgSrvAck(dg_client *cli, uint32_t ack, uint32_t ts, int wnd, int wndFlag, int line)
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

    printf("[Debug@%d #%d]: Send ACK #%d [ack=%d seq=%d ts=%d win=%d]\n", 
        line, pthread_self(), dg.ack, dg.ack, dg.seq, dg.ts, dg.wnd);
}

void SendDgSrvFin(dg_client *cli, uint32_t ack)
{
    struct filedatagram dg;
    // init filedatagram
    bzero(&dg, sizeof(dg));
    dg.seq = cli->seq++;
    dg.ack = ack;
    dg.flag.eof = 1;
    dg.len = 0;

    if (DgRandom() > cli->arg->p)
        Dg_writepacket(cli->sock, &dg);
}

// thread of Print file content
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
    double d = 0.0; 
    struct timeval tv;
    struct filedatagram fd;

    printf("[Client]: Print thread #%d is working\n", pthread_self());
    
    while (1)
    {
        // get data from fifo
        ret = ReadDgFifo(cli->fifo, &fd, &size);
        if (ret < 0)        
        {
            // produce a random double in the range (0.0, 1.0) 
            d = ((rand() + 1) / (double)(RAND_MAX + 2));
            tv.tv_sec = 0;
            tv.tv_usec = -1 * cli->arg->u * log(d);  // -1 * u * ln(random())
            select(0, NULL, NULL, NULL, &tv);
            continue;
        }

        //printf("[Thread #%d]: Read fifo, seq=%d ack=%d ts=%d wnd=%d flag.eof=%d len=%d" \
            "\n--------------------\n%s\n--------------------\n", \
            pthread_self(), fd.seq, fd.ack, fd.ts, fd.wnd, fd.flag.eof, fd.len, fd.data);

        if (fd.flag.eof == 1)
        {
            printf("[Thread #%d]: File data finished\n", pthread_self());
            break;
        }            
    }

    printf("[Client]: Print thread #%d exited\n", pthread_self());

    //SendDgSrvFin(cli, fd.seq + 1);

    exit(0);
}

// create a thread
void CreateThread(dg_client *cli)
{
    pthread_t tid;
    Pthread_create(&tid, NULL, &PrintOutThread, cli);

    printf("[Client]: Create thread ok, tid=%d\n", tid);
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
            if (errno == ETIMEDOUT || errno == EAGAIN)
            {
                printf("[Client]: Send filename req to server %s:%d error: %s\n", cli->arg->srvIP, cli->arg->srvPort, strerror(errno));
                continue;
            }
            
            printf("[Client]: Connect server %s:%d error\n", cli->arg->srvIP, cli->arg->srvPort);
            return -1;
        }

        // reconnect server with new port number
        ReconnectDgSrv(cli);
        
        // send port ack
        ret = SendDgSrvNewPortAck(cli, &dg);
        if (ret < 0)
        {
            if (errno == ETIMEDOUT || errno == EAGAIN)
            {
                printf("[Client]: Send port ack to server %s:%d error: %s\n", cli->arg->srvIP, cli->newPort, strerror(errno));
                // reconnect well known port number
                cli->newPort = cli->arg->srvPort;
                ReconnectDgSrv(cli);
                continue;
            }
        }
    } while (ret < 0);

    // save first segment
    WriteDgRcvBuf(cli->buf, &dg);

    return 0;
}

// get datagram from buffer
// 1. check the receiver window
// 2. if there is in-order segments, put those segments to fifo
// 3. if there is more than 2 in-order segments, send ack to server   
void GetDatagram(dg_client *cli, int need)
{
    int ret = 0;
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
        SendDgSrvAck(cli, dg.seq + 1, 0/*dg.ts*/, cli->buf->rwnd.win, 1, __LINE__);
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
        ret = RecvDataTimeout(cli->sock, &dg, &sz, cli->timeout, cli->arg->p);
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
        dg.ts = ntohl(dg.ts);
        dg.wnd = ntohs(dg.wnd);
#endif


        //printf("dg.seq=%d ret=%d, rwnd.base=%d rwnd.next=%d rwnd.top=%d\n", \
            dg.seq, ret, cli->buf->rwnd.base, cli->buf->rwnd.next, cli->buf->rwnd.top);

        // received window probe
        if (dg.flag.pob == 1)
        {
            // send current window size
            SendDgSrvAck(cli, cli->buf->nextSeq, dg.ts, cli->buf->rwnd.win, 1, __LINE__);
            continue;
        }        
        
        uint32_t ack = 0, ts = 0;
        // put data to receive buffer
        ack = WriteDgRcvBuf(cli->buf, &dg);
        if (ack == -1)
        {
            // sliding window size is zero
            SendDgSrvAck(cli, cli->buf->nextSeq, dg.ts, cli->buf->rwnd.win, 1, __LINE__);
            continue;
        }
        else if (ack == -2 || ack == -3)
            continue;

        if (ack > 0)
        {
            // out of order, send duplicate ack
            SendDgSrvAck(cli, ack, cli->buf->ts, cli->buf->rwnd.win, 0, __LINE__);
        }

        // get datagram from buffer
        if (GetInOrderAck(cli->buf, &ack, &ts) == 0)
        {
            SendDgSrvAck(cli, ack, ts, cli->buf->rwnd.win, 0, __LINE__);
        }
    }

    return 0;
}
