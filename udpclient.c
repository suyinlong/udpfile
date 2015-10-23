/*
* @Author: Yinlong Su
* @Date:   2015-10-09 09:49:37
* @Last Modified by:   Yinlong Su
* @Last Modified time: 2015-10-22 23:28:32
*
* File:         udpclient.c
* Description:  Client C file
*/

#include "udpfile.h"
#include "dgcli_impl.h"

char    IPserver[IP_BUFFSIZE], IPclient[IP_BUFFSIZE];
int     port = 0;
char    filename[FILENAME_BUFFSIZE];
int     max_winsize = 0;
int     seed = 0;
double  p = 0.0;
int     mu = 0;

/* --------------------------------------------------------------------------
 *  readArguments
 *
 *  Read arguments from file
 *
 *  @param  : void
 *  @return : void
 *  @see    : file#client.in,
 *            var#IPserver, var#port,
 *            var#filename,
 *            var#max_winsize, var#seed, var#p, var#mu
 *
 *  Read arguments from "client.in"
 *    Line 1: <STRING>      -> char IPserver[IP_BUFFSIZE]
 *    Line 2: <INTEGER>     -> int port
 *    Line 3: <STRING>      -> char filename[FILENAME_BUFFSIZE]
 *    Line 4: <INTEGER>     -> int max_winsize
 *    Line 5: <INTEGER>     -> int seed
 *    Line 6: <DOUBLE>      -> double p
 *    Line 7: <INTEGER>     -> int mu
 * --------------------------------------------------------------------------
 */
void readArguments() {
    int     i;
    FILE    *fp;
    fp = Fopen("client.in", "rt");

    bzero(IPserver, IP_BUFFSIZE);
    bzero(filename, FILENAME_BUFFSIZE);

    Fgets(IPserver, IP_BUFFSIZE, fp);
    fscanf(fp, "%d\n", &port);
    Fgets(filename, FILENAME_BUFFSIZE, fp);
    fscanf(fp, "%d", &max_winsize);
    fscanf(fp, "%d", &seed);
    fscanf(fp, "%lf", &p);
    fscanf(fp, "%d", &mu);

    // clear the enter characters at the end of IPserver and filename
    for (i = 0; i < strlen(IPserver); i++)
        if (IPserver[i] < 32) IPserver[i] = 0;
    for (i = 0; i < strlen(filename); i++)
        if (filename[i] < 32) filename[i] = 0;

    printf("[client.in] address=%s:%d, filename=%s, max_winsize=%d, seed=%d, p=%lf, mu=%d\n", IPserver, port, filename, max_winsize, seed, p, mu);
    Fclose(fp);
}

/* --------------------------------------------------------------------------
 *  prifinfo_plus
 *
 *  Print interfaces information of client
 *
 *  @param  : void
 *  @return : struct ifi_info *
 *  @see    : function#Get_ifi_info_plus
 *
 *  Use Get_ifi_info_plus() to get interfaces information
 *  Print out the information
 *  Return the ifi_info structure pointer
 * --------------------------------------------------------------------------
 */
struct ifi_info *prifinfo_plus() {
    struct ifi_info *ifi, *ifihead;
    struct sockaddr *sa;
    u_char  *ptr;
    int     i;

    for (ifihead = ifi = Get_ifi_info_plus(AF_INET, 1); ifi != NULL; ifi = ifi->ifi_next) {
        printf("%s ", ifi->ifi_name);
        if (ifi->ifi_index != 0)
            printf("(%d) ", ifi->ifi_index);

        // print flags
        printf("<");
        if (ifi->ifi_flags & IFF_UP)            printf("UP ");
        if (ifi->ifi_flags & IFF_BROADCAST)     printf("BCAST ");
        if (ifi->ifi_flags & IFF_MULTICAST)     printf("MCAST ");
        if (ifi->ifi_flags & IFF_LOOPBACK)      printf("LOOP ");
        if (ifi->ifi_flags & IFF_POINTOPOINT)   printf("P2P ");
        printf(">\n");

        if ( (i = ifi->ifi_hlen) > 0) {
            ptr = ifi->ifi_haddr;
            do {
                printf("%s%x", (i == ifi->ifi_hlen) ? "  " : ":", *ptr++);
            } while(--i > 0);
            printf("\n");
        }

        if (ifi->ifi_mtu != 0)
            printf("  MTU: %d\n", ifi->ifi_mtu);
        if ( (sa = ifi->ifi_addr) != NULL)
            printf("  IP addr: %s\n", Sock_ntop_host(sa, sizeof(*sa)));
        if ( (sa = ifi->ifi_ntmaddr) != NULL)
            printf("  network mask: %s\n", Sock_ntop_host(sa, sizeof(*sa)));
        if ( (sa = ifi->ifi_brdaddr) != NULL)
            printf("  broadcast addr: %s\n", Sock_ntop_host(sa, sizeof(*sa)));
        if ( (sa = ifi->ifi_dstaddr) != NULL)
            printf("  destination addr: %s\n", Sock_ntop_host(sa, sizeof(*sa)));
    }

    return ifihead;
}

/* --------------------------------------------------------------------------
 *  designateAddr
 *
 *  Designate IPserver and IPclient address
 *
 *  @param  : struct ifi_info *
 *  @return : int
 *
 *  Check if server is on the same host (loopback)
 *  Check if server is on the same subnet (match the longest prefix)
 *  Designate IPserver and IPclient address and return whether it is local
 * --------------------------------------------------------------------------
 */
int designateAddr(struct ifi_info *ifihead) {
    struct ifi_info *ifi;
    struct sockaddr *addr, *ntmaddr;
    unsigned long   max_match = 0, ntm;
    unsigned long   subnserver, subnclient;

    bzero(IPclient, IP_BUFFSIZE);

    // check if local (loopback)
    for (ifi = ifihead; ifi != NULL; ifi = ifi->ifi_next) {
        if ( ((addr = ifi->ifi_addr) != NULL) && (strcmp(IPserver, Sock_ntop_host(addr, sizeof(*addr))) == 0) ) {
            strcpy(IPserver, "127.0.0.1");
            strcpy(IPclient, "127.0.0.1");
            return 1;
        }
    }

    // check if local (subnets)
    for (ifi = ifihead; ifi != NULL; ifi = ifi->ifi_next) {
        if ( ((addr = ifi->ifi_addr) != NULL) && ((ntmaddr = ifi->ifi_ntmaddr) != NULL)) {
            ntm = inet_addr(Sock_ntop_host(ntmaddr, sizeof(*ntmaddr)));
            subnserver = inet_addr(IPserver) & ntm;
            subnclient = inet_addr(Sock_ntop_host(addr, sizeof(*addr))) & ntm;
            if (subnserver == subnclient && ntm > max_match) {
                max_match = ntm;
                bzero(IPclient, IP_BUFFSIZE);
                strcpy(IPclient, Sock_ntop_host(addr, sizeof(*addr)));
            }
        }
    }

    if (max_match > 0) return 1;

    // choose random IPclient, not local
    for (ifi = ifihead; ifi != NULL; ifi = ifi->ifi_next) {
        if ( ((addr = ifi->ifi_addr) != NULL) && !(ifi->ifi_flags & IFF_LOOPBACK)) {
            strcpy(IPclient, Sock_ntop_host(addr, sizeof(*addr)));
            return 0;
        }
    }

    return 0;
}

/* --------------------------------------------------------------------------
 *  main
 *
 *  Entry function
 *
 *  @param  : int   argc
 *            char  **argv
 *  @return : int
 *  @see    : function#readArguments,
 *            function#prifinfo_plus,
 *            function#designateAddr
 *            function#Dg_cli
 *
 *  Server entry function
 * --------------------------------------------------------------------------
 */
int main(int argc, char **argv) {
    const int   on = 1;
    int         local, sockfd;
    socklen_t   len;
    struct ifi_info         *ifihead;
    struct sockaddr_in      servaddr, clientaddr;
    struct sockaddr_storage ss;

    readArguments();
    ifihead = prifinfo_plus();

    local = designateAddr(ifihead);
    free_ifi_info_plus(ifihead);

    printf("LOCAL=%d, IPserver=%s, IPclient=%s\n", local, IPserver, IPclient);

    sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
    //Setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if (local)
        Setsockopt(sockfd, SOL_SOCKET, SO_DONTROUTE, &on, sizeof(on));

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    Inet_pton(AF_INET, IPserver, &servaddr.sin_addr);

    bzero(&clientaddr, sizeof(clientaddr));
    clientaddr.sin_family = AF_INET;
    //clientaddr.sin_port = htons(0);
    Inet_pton(AF_INET, IPclient, &clientaddr.sin_addr);

    // bind the clientaddr to socket
    Bind(sockfd, (SA *)&clientaddr, sizeof(clientaddr));

    // getsockname of client part
    len = sizeof(ss);
    if (getsockname(sockfd, (SA *) &ss, &len) < 0) {
        printf("getsockname error\n");
        return (-1);
    }

    // output socket information
    struct sockaddr_in *sockaddr = (struct sockaddr_in *)&ss;
    printf("UDP Client Socket: %s:%d\n", inet_ntoa(sockaddr->sin_addr), sockaddr->sin_port);

    // connect
    Connect(sockfd, (SA *)&servaddr, sizeof(servaddr));

    // getpeername of server part
    if (getpeername(sockfd, (SA *) &ss, &len) < 0) {
        printf("getpeername error\n");
        return (-1);
    }

    // output peer information
    printf("UDP Server Socket: %s:%d\n", inet_ntoa(sockaddr->sin_addr), sockaddr->sin_port);

#if 0
    Dg_cli(sockfd);
#elif 1
    // initial client arguments
    dg_arg arg;
    bzero(&arg, sizeof(arg));
    strcpy(arg.srvIP, IPserver);
    arg.srvPort = sockaddr->sin_port;
    strcpy(arg.filename, filename);
    arg.rcvWin = max_winsize;
    arg.seed = seed;
    arg.p = p;
    arg.u = mu;

    // create a client
    dg_client *cli = CreateDgCli(&arg, sockfd);

    // start the client
    StartDgCli(cli);

    // destroy the client
    DestroyDgCli(cli);
#endif
    exit(0);
}

