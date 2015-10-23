/**
* @file         :  dgcli_impl.c
* @author       :  Jiewen Zheng
* @date         :  2015-10-13
* @brief        :  UDP datagram client implementation
* @changelog    :
**/

//#include <signal.h>
#include <math.h>

#include "dgcli_impl.h"

#define  RCV_TIMEOUT    5   // 5sec


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
    // destroy the receive buffer
    DestroyDgRcvBuf(cli->buf);

    // destroy the fifo
    DestroyDgFifo(cli->fifo);

    // free the client resource
    if (cli)
    {
        free(cli);
        cli = NULL;
    }
}

static void HandleTimeout(int signo)
{
    // just interrupt the operation
    return;		
}

void ReconnectDgSrv(dg_client *cli)
{
    struct sockaddr_in srvAddr;

    // reconnect
    bzero(&srvAddr, sizeof(srvAddr));
    srvAddr.sin_family = AF_INET;
    srvAddr.sin_port = htons(cli->newPort);
    Inet_pton(AF_INET, cli->arg->srvIP, &srvAddr.sin_addr);

    Connect(cli->sock, (SA *)&srvAddr, sizeof(srvAddr));
}


int RecvDgSrvFilenameAck(dg_client *cli)
{
    struct filedatagram fdata;

    // init filedatagram
    bzero(&fdata, sizeof(fdata));

    Dg_readpacket(cli->sock, &fdata);

    if (fdata.flag.pot != 1) 
    {
        printf("[Client]: Received an invalid packet (no port number).\n");
        return -1;
    }
    else 
    {
        cli->newPort = atoi(fdata.data);
        printf("[Client]: Received a valid private port number %d from server.\n", cli->newPort);

        ReconnectDgSrv(cli);
    }

    return 0;
}

int SetAlarmTimer(int t, Sigfunc *sigfunc)
{
    // set alarm
    sigfunc = Signal(SIGALRM, HandleTimeout);
    if (alarm(t) != 0)
    {
        printf("[Client]: Alarm was already set.\n");
        return -1;
    }

    return 0;
}

void TurnOffAlarmTimer(Sigfunc *sigfunc)
{
    // turn off the alarm
    alarm(0);
    // restore previous signal handler
    Signal(SIGALRM, sigfunc);	
}

int alarm_counter = 0;
void alarm_handler(int signal) 
{
    alarm_counter++;
}

void setup_alarm_handler() 
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = alarm_handler;
    //sa.flags = 0;
    if (sigaction(SIGALRM, &sa, 0) < 0)
        printf("Can't establish signal handler");
}

int RecvDataTimeout(int fd, void *data, int *size, int timeout)
{
    Sigfunc *sigfunc;
    // set alarm
    sigfunc = Signal(SIGALRM, HandleTimeout);
    if (alarm(timeout) != 0)
    {
        printf("[Client]: Alarm was already set.\n");
        return -1;
    }

    int	ret = 0;
    // read data
    if ((ret = read(fd, data, *size)) < 0)
    {
        if (errno == EINTR)
            errno = ETIMEDOUT;
    }

    // turn off the alarm
    alarm(0);
    // restore previous signal handler
    Signal(SIGALRM, sigfunc);

    return ret;
}

int SendDgSrvFilenameReq(dg_client *cli)
{
    struct filedatagram sndData, rcvData;
    // init filedatagram
    bzero(&sndData, sizeof(sndData));
    bzero(&rcvData, sizeof(rcvData));

    sndData.seq = cli->seq;
    sndData.flag.fln = 1;
    sndData.len = strlen(cli->arg->filename);
    strcpy(sndData.data, cli->arg->filename);

    while (1)
    {
        // if random() <= p, discard the datagram (just don't send)
        if (random() > cli->arg->p)
            Dg_writepacket(cli->sock, &sndData);

        // wait for server data, use a timer
        int rcvSize = sizeof(rcvData);
        if (RecvDataTimeout(cli->sock, &rcvData, &rcvSize, cli->timeout) < 0)
        {
            if (errno != ETIMEDOUT)
                return -1; // error
        }
        else
        {
            // successfully receive data from server
            break;
        }
    }

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

void SendDgSrvAck(dg_client *cli, uint32_t ack, uint32_t ts, int wnd)
{
    struct filedatagram sndData;
    // init filedatagram
    bzero(&sndData, sizeof(sndData));

    sndData.seq = cli->seq++;
    sndData.ack = ack;
    sndData.ts = ts;
    sndData.flag.wnd = wnd;
    sndData.len = 0;

    if (random() > cli->arg->p)
        Dg_writepacket(cli->sock, &sndData);
}

int SendDgSrvNewPortAck(dg_client *cli, struct filedatagram *data)
{
    struct filedatagram sndData;
    // init filedatagram
    bzero(&sndData, sizeof(sndData));
    sndData.seq = cli->seq++;
    sndData.ack = 1;
    sndData.flag.pot = 1;
    sndData.len = 0;

    while (1)
    {
        // if random() <= p, discard the datagram (just don't send)
        if (random() > cli->arg->p)
            Dg_writepacket(cli->sock, &sndData);

        // wait for server data, use a timer
        int rcvSize = sizeof(struct filedatagram);
        if (RecvDataTimeout(cli->sock, data, &rcvSize, cli->timeout) < 0)
        {
            if (errno != ETIMEDOUT)
                return -1; // error
        }
        else
        {
            // successfully receive data from server
            break;
        }
    }

    return 0;
}

int SendAck(dg_client *cli, int ack, int ts, int win)
{
    struct filedatagram sndData;
    // init filedatagram
    bzero(&sndData, sizeof(sndData));

    sndData.flag.wnd = 1;
    sndData.seq = cli->seq++;
    sndData.ack = ack;
    sndData.ts = ts;
    sndData.wnd = win;

    if (random() > cli->arg->p)
        Dg_writepacket(cli->sock, &sndData);

    return 0;
}

void *PrintOutThread(void *arg)
{
    if (arg == NULL)
    {
        printf("[Client]: Print out work thread error.\n");
        return;
    }
    
    dg_client *cli = (dg_client *)arg;

    char *data;
    int size = 0;
    int ret = 0;
    double d = 0.0; 
    struct timeval tv;
    
    while (1)
    {
        ret = ReadDgFifo(cli->fifo, data, &size);
        if (ret < 0)        
        {
            d = ((rand() % 10000) / 10000.0);
            tv.tv_sec = 0;
            tv.tv_usec = -1 * cli->arg->u * log(d);  // -1 * u * ln(random())
            select(0, NULL, NULL, NULL, &tv);
            continue;
        }            

        struct filedatagram *fdata = (struct filedatagram *)data;

        printf("[Client]: seq=%d timestamp=%d win=%d\n", fdata->seq, fdata->ts, fdata->wnd);

        printf("[Client]: File data\n%s\n", fdata->data);

        if (fdata->flag.eof)
        {
            printf("[Client]: File data finish\n", fdata->data);
            break;
        }            
    }
}

void CreateThread(dg_client *cli)
{
    pthread_t tid;
    Pthread_create(&tid, NULL, &PrintOutThread, cli);

    printf("[Client]: Create thread ok, tid=%d\n", tid);
}


void SetDelayedAckTimer(dg_client *cli)
{

}

int ConnectDgServer(dg_client *cli)
{
    int ret = 0;
    struct filedatagram dg;

    do
    {
        // connect server
        ret = SendDgSrvFilenameReq(cli);
        if (ret < 0)
        {
            printf("[Client]: Connect server error.\n");
            return -1;
        }

        // reconnect server with new port number
        ReconnectDgSrv(cli);
        
        // send connect ack
        ret = SendDgSrvNewPortAck(cli, &dg);
    } while (ret < 0);

    // save first segment
    ret = WriteDgRcvBuf(cli->buf, &dg);

    return 0;
}

int StartDgCli(dg_client *cli)
{
    if (NULL == cli)
        return -1;

    int ret = 0;
    struct filedatagram fd1, fd2;

    // connect server
    if (ConnectDgServer(cli) < 0)
        return -1;

    // create print out thread
    CreateThread(cli);

    // set and start delayed ack timer
    SetDelayedAckTimer(cli);

    while (1)
    {
        int sz = sizeof(fd1);
        // receive data
        ret = RecvDataTimeout(cli->sock, &fd1, &sz, cli->timeout);
        if (ret < 0)
        {
            if (errno == ETIMEDOUT)
                continue;
            else
                break;
        }
        
        fd1.len = ntohs(fd1.len);
        fd1.seq = ntohl(fd1.seq);
        fd1.ack = ntohl(fd1.ack);
        fd1.ts = ntohl(fd1.ts);
        fd1.wnd = ntohs(fd1.wnd);

        // put data to receive buffer
        ret = WriteDgRcvBuf(cli->buf, &fd1);
        if (ret < 0)
            continue;
        else if (ret > 0)
        {
            // out of order, send duplicate ack
            SendAck(cli, ret, fd2.ts, cli->buf->rwnd.win);
        }  

        int need = 0;
        do
        {
            // get data from receive buffer
            ret = ReadDgRcvBuf(cli->buf, &fd2, need);
            if (ret > 0)
            {
                // put data to fifo
                WriteDgFifo(cli->fifo, &fd2, sizeof(fd2));
            }
            need = ret;
        } while (ret > 0);

        if (ret != -1)
            SendAck(cli, fd2.seq+1, fd2.ts, cli->buf->rwnd.win);
    }

    return 0;
}
