#ifndef __udpfile_h
#define __udpfile_h

#include "unp.h"
#include "unpthread.h"
#include "unpifiplus.h"

#define SOCK_NOTINIT   -1

struct socket_info {
    int sockfd;
    struct sockaddr     *addr;      /* primary address */
    struct sockaddr     *ntmaddr;   /* netmask address */
    struct sockaddr     *subnaddr;  /* subnet address */
    struct socket_info  *next;      /* next of these structures */
};

typedef unsigned char BITFIELD8;
typedef struct {
    /* common flags */
    BITFIELD8   ack : 1; /* ack flag */
    BITFIELD8   eof : 1; /* eof flag */
    BITFIELD8   fln : 1; /* filename flag */
    BITFIELD8   pot : 1; /* port flag */
    BITFIELD8   r04 : 1;
    BITFIELD8   r05 : 1;
    BITFIELD8   r06 : 1;
    BITFIELD8   r07 : 1;
} DATAGRAM_STATUS;

#define DATAGRAM_PAYLOAD    512
#define DATAGRAM_HEADERSIZE (3 * sizeof(unsigned int) + sizeof(DATAGRAM_STATUS))
#define DATAGRAM_DATASIZE   (DATAGRAM_PAYLOAD - DATAGRAM_HEADERSIZE)

struct filedatagram {
    unsigned int    seq;
    unsigned int    timestamp;
    unsigned int    datalength;
    DATAGRAM_STATUS flag;
    char            data[DATAGRAM_DATASIZE];
};

// Buffer size definition
#define IP_BUFFSIZE         20
#define FILENAME_BUFFSIZE   255

// function headers
extern struct ifi_info *Get_ifi_info_plus(int family, int doaliases);
extern        void      free_ifi_info_plus(struct ifi_info *ifihead);

void Dg_writepacket(int, const struct filedatagram *);
void Dg_readpacket(int, struct filedatagram *);

unsigned int getDGSequence(struct filedatagram);
unsigned int getSequence(char *);

DATAGRAM_STATUS getDGFlag(struct filedatagram);
DATAGRAM_STATUS getFlag(char *);

char *getDGData(struct filedatagram);
char *getData(char *);


void Dg_cli(int, const SA *, socklen_t);

#endif
