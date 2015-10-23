#ifndef __udpfile_h
#define __udpfile_h

#include "unp.h"
#include "unpthread.h"
#include "unpifiplus.h"

// Socket on all unicast interfaces structure: socket_info

#define SOCK_NOTINIT   -1

struct socket_info {
    int sockfd;
    struct sockaddr     *addr;      /* primary address */
    struct sockaddr     *ntmaddr;   /* netmask address */
    struct sockaddr     *subnaddr;  /* subnet address */
    struct socket_info  *next;      /* next of these structures */
};

// File datagram sturcture

typedef unsigned char BITFIELD8;
typedef struct {
    /* common flags */
    BITFIELD8   eof : 1; /* eof flag */
    BITFIELD8   fln : 1; /* filename flag */
    BITFIELD8   pot : 1; /* port flag */
    BITFIELD8   wnd : 1; /* window update flag */
    BITFIELD8   pob : 1; /* window probe flag */
    BITFIELD8   r05 : 1;
    BITFIELD8   r06 : 1;
    BITFIELD8   r07 : 1;
} DATAGRAM_STATUS;

#define DATAGRAM_PAYLOAD    512
#define DATAGRAM_HEADERSIZE (3 * sizeof(uint32_t) + 2 * sizeof(uint16_t) + sizeof(DATAGRAM_STATUS))
#define DATAGRAM_DATASIZE   (DATAGRAM_PAYLOAD - DATAGRAM_HEADERSIZE)

struct filedatagram {
    uint32_t    seq;
    uint32_t    ack;
    uint32_t    ts;
    uint16_t    wnd;
    uint16_t    len;
    DATAGRAM_STATUS flag;
    char            data[DATAGRAM_DATASIZE];
};

// Server sender windows structure

struct sender_window {
    struct filedatagram     datagram;
    struct sender_window    *next;
};

// Buffer size definition
#define IP_BUFFSIZE         20
#define FILENAME_BUFFSIZE   255

// Server connected processes sturcture

struct process_info {
    pid_t   pid;
    char    filename[FILENAME_BUFFSIZE];
    char    address[IP_BUFFSIZE];
    int     port;
    struct process_info *next;
};

// Congestion Control
#define CC_IWND     1   // default iwnd (initial window)
#define CC_SSTHRESH -1  // default ssthresh, -1 indicate that ssthresh = awnd

// function headers
extern struct ifi_info *Get_ifi_info_plus(int family, int doaliases);
extern        void      free_ifi_info_plus(struct ifi_info *ifihead);

void Dg_recvpacket(int, struct sockaddr *, socklen_t *, struct filedatagram *);

void Dg_writepacket(int, const struct filedatagram *);
void Dg_readpacket(int, struct filedatagram *);

void Dg_cli(int);

void Dg_serv(int, struct socket_info *, struct sockaddr *, struct sockaddr *, char *, int);

void cc_timeout();
void cc_init(uint16_t, uint16_t);
uint16_t cc_wnd();
uint16_t cc_updwnd(uint16_t);
uint16_t cc_ack(uint32_t, uint16_t, uint8_t*);


#endif
