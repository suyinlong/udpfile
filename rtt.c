#include "unprtt.h"

/*
 * Calculate the RTO value based on current estimators:
 * RTO = (srtt >> 3) + rttvar
 */
#define	RTT_RTOCALC(ptr) (((ptr)->rtt_srtt >> 3) + ((ptr)->rtt_rttvar))

/*
 * Wrapper function to make rto between [RTT_RXTMIN, RTT_RXTMAX]
 * Return rto value in milliseconds
 */
static uint32_t rtt_minmax(uint32_t rto) {
    if (rto < RTT_RXTMIN)
        rto = RTT_RXTMIN;
    else if (rto > RTT_RXTMAX)
        rto = RTT_RXTMAX;
    return(rto);
}

/*
 * Init rtt mechanism
 */
void rtt_init(struct rtt_info *ptr) {
    struct timeval tv;

    Gettimeofday(&tv, NULL);
    ptr->rtt_base = tv.tv_sec;

    ptr->rtt_rtt    = 0;
    ptr->rtt_srtt   = 0;
    ptr->rtt_rttvar = 3000;
    ptr->rtt_rto = rtt_minmax(RTT_RTOCALC(ptr));
    /* first RTO at ((srtt >> 3) + rttvar) = 3000 milliseconds */
}

/*
 * Return the current timestamp.
 * Our timestamps are 32-bit integers that count milliseconds since
 * rtt_init() was called.
 */
uint32_t rtt_ts(struct rtt_info *ptr) {
    uint32_t        ts;
    struct timeval  tv;

    Gettimeofday(&tv, NULL);
    ts = ((tv.tv_sec - ptr->rtt_base) * 1000) + (tv.tv_usec / 1000);
    return(ts+1);
}

void rtt_newpack(struct rtt_info *ptr) {
    ptr->rtt_nrexmt = 0;
}

uint32_t rtt_start(struct rtt_info *ptr) {
    return(ptr->rtt_rto);
}

void rtt_stop(struct rtt_info *ptr, uint32_t ms) {
    double delta;

    ptr->rtt_rtt = ms; /* measured RTT in milliseconds */

/*
 * rtt_srtt is stored in a scaled-up form, at eight times its real value
 * rtt_rttvar is stored in a scaled-up form, at four times its real value
 */
    ptr->rtt_rtt -= (ptr->rtt_srtt >> 3);
    ptr->rtt_srtt += ptr->rtt_rtt;
    if (ptr->rtt_rtt < 0)
        ptr->rtt_rtt = - ptr->rtt_rtt;
    ptr->rtt_rtt -= (ptr->rtt_rttvar >> 2);
    ptr->rtt_rttvar += ptr->rtt_rtt;
    ptr->rtt_rto = (ptr->rtt_srtt >> 3) + ptr->rtt_rttvar;

    ptr->rtt_rto = rtt_minmax(RTT_RTOCALC(ptr));

}

/*
 * Return -1 if timeout times more than RTT_MAXNREXMT
 */
int rtt_timeout(struct rtt_info *ptr) {
    ptr->rtt_rto <<= 1; /* next RTO */
    ptr->rtt_rto = rtt_minmax(ptr->rtt_rto);

    if (++ptr->rtt_nrexmt > RTT_MAXNREXMT)
        return(-1);
    return(0);
}
