/*
* @Author: Yinlong Su
* @Date:   2015-10-13 10:01:16
* @Last Modified by:   Yinlong Su
* @Last Modified time: 2015-10-22 23:15:07
*
* File:         rtserv.c
* Description:  Reliable Transmission Server C file
*/

#include "udpfile.h"

uint32_t    pid = 0;
uint32_t    last_ack;   // last ACKed sequence number
uint32_t    this_ack;   // this ACKed sequence number
uint32_t    dup_c;      // duplicate ACK counter
uint8_t     fast_rec;   // fast recovery flag

uint16_t    awnd;       // client's advertised window
uint16_t    iwnd;       // initial window
uint16_t    mwnd;       // max window
uint16_t    cwnd;       // congestion window

uint16_t    ssthresh;   // slow start threshold
uint16_t    ca_c;       // congestion avoidance counter


/* --------------------------------------------------------------------------
 *  congestion_avoidance
 *
 *  Congestion Avoidance algorithm
 *
 *  @param  : void
 *  @return : void
 *
 *  # This is a static inline function
 *  Congestion Avoidance algorithm in Congestion Control
 *  lin- with respect to time
 *  Modification in A2:
 *      Add a new variable ca_c (congestion avoidance counter).
 *      When ca_c reachs cwnd, the cwnd will add by 1 and ca_c will be set
 *      to 0.
 * --------------------------------------------------------------------------
 */
static inline void congestion_avoidance() {
    // for a nonduplicate ACK, congestion avoidance updates cwnd(bytes) as:
    //     cwnd <- cwnd + SMSS*SMSS/cwnd
    // for our assignment, cwnd is integer(number of datagram):
    //     cwnd <- cwnd + 1/cwnd
    // that means if cwnd = K, we need K good ACKs to add cwnd by 1
    // use ca_c to remember how many good ACKs we have already
    ca_c += this_ack - last_ack;
    while (ca_c >= cwnd) {
        ca_c -= cwnd;
        cwnd ++;
    }
    printf("[Server Child #%d]: CC Congestion Avoidance. (cwnd = %d, ca_c = %d)\n", pid, cwnd, ca_c);
}

/* --------------------------------------------------------------------------
 *  slow_start
 *
 *  Slow Start algorithm
 *
 *  @param  : void
 *  @return : void
 *  @see    : function#congestion_avoidance
 *
 *  # This is a static inline function
 *  Slow Start algorithm in Congestion Control
 *  lin- with respect to time
 *  Modification in A2:
 *      If new ACK will cause cwnd exceeds ssthresh in slow start algorithm,
 *      this function will split the process into slow start phase and
 *      congestion avoidance phase
 * --------------------------------------------------------------------------
 */
static inline void slow_start() {
    // for a good ACK, slow start operates by incrementing cwnd by N
    // N is the number of previously unacknowledged bytes ACKed by the received "good" ACK
    if (cwnd + this_ack - last_ack > ssthresh) {
        // If after this ACKs, cwnd will go above ssthresh
        // split the process to slow start phase and congestion avoidance phase
        last_ack += ssthresh - cwnd;
        cwnd = ssthresh;
        ca_c = 0;
        congestion_avoidance();
    } else {
        // cwnd is still within ssthresh
        cwnd += this_ack - last_ack;
    }

    printf("[Server Child #%d]: CC Slow Start. (cwnd = %d)\n", pid, cwnd);
}

/* --------------------------------------------------------------------------
 *  cc_timeout
 *
 *  Congestion Control timeout function
 *
 *  @param  : void
 *  @return : void
 *
 *  Timeout function
 *  If one datagram is timeout, do the following:
 *      ssthresh = cwnd / 2
 *      cwnd = iwnd (1 MSS)
 *      dupACKcount = 0
 *      # retransmit missing datagram (in dgserv.c)
 *      next state is slow slart
 * --------------------------------------------------------------------------
 */
void cc_timeout() {
    ssthresh = cwnd >> 1;
    cwnd = iwnd;
    dup_c = 0;
    ca_c = 0;

    printf("[Server Child #%d]: Congestion Control Timeout. (cwnd = %d, ssthresh = %d)\n", pid, cwnd, ssthresh);
}

/* --------------------------------------------------------------------------
 *  cc_init
 *
 *  Congestion Avoidance initialization
 *
 *  @param  : uint16_t  : advertised receiver window size
 *            uint16_t  : max sender window size
 *  @return : void
 *
 *  Congestion Avoidance initialization
 *
 *  Set relevant variables to 1 or 0
 *  Accept awnd and mwnd from the arguments
 *  cwnd is set to iwnd (CC_IWND is defined in udpfile.h) at beginning
 *  ssthresh is set to awnd by default (or CC_SSTHRESH defined in udpfile.h)
 * --------------------------------------------------------------------------
 */
void cc_init(uint16_t advertised_wnd, uint16_t max_wnd) {
    last_ack    = 1;
    this_ack    = 1;
    dup_c       = 0;
    fast_rec    = 0;
    ca_c        = 0;

    awnd = advertised_wnd;
    mwnd = max_wnd;
    iwnd = CC_IWND;
    cwnd = iwnd;

    if (CC_SSTHRESH < 0)
        ssthresh = awnd;
    else
        ssthresh = CC_SSTHRESH;

    printf("[Server Child #%d]: CC initialized. (awnd = %d, mwnd = %d, iwnd = %d, cwnd = %d, ssthresh = %d)\n", pid, awnd, mwnd, iwnd, cwnd, ssthresh);
}

/* --------------------------------------------------------------------------
 *  cc_wnd
 *
 *  Congestion Control window size
 *
 *  @param  : void
 *  @return : uint16_t  : the number of datagrams that can be sent
 *
 *  Congestion Control window size function
 *
 *  Return the min value of cwnd and awnd
 * --------------------------------------------------------------------------
 */
uint16_t cc_wnd() {
    return min(cwnd, awnd);
}

/* --------------------------------------------------------------------------
 *  cc_updwnd
 *
 *  Congestion Control update window size
 *
 *  @param  : uint16_t  : advertised receiver window size
 *  @return : uint16_t  : the number of datagrams that can be sent
 *
 *  Congestion Control update window size function
 *
 *  Use to update awnd when server received the return window update datagram
 *      (with flag.wnd == 1, defined in udpfile.h)
 *  Return the min value of cwnd and awnd
 * --------------------------------------------------------------------------
 */
uint16_t cc_updwnd(uint16_t wnd) {
    awnd = wnd;
    return min(cwnd, awnd);
}

/* --------------------------------------------------------------------------
 *  cc_ack
 *
 *  Congestion Control Acknowledgements Handle function
 *
 *  @param  : uint32_t  : ACK sequence number
 *  @return : uint16_t  : advertised receiver window size
 *
 *  Congestion Control Acknowledgements Handler
 *
 *  1. Update awnd
 *  2. Calculate the duplicate times
 *  3. (a) If duplicate counter is greater than 3 and server is in fast
 *         recovery state:
 *             cwnd is increased by 1
 *     (b) If duplicate counter is equal with 3, server goes into fast
 *         recovery state:
 *             ssthresh = cwnd / 2
 *             cwnd = ssthresh + 3 (the window-inflation, not needed in A2)
 *             # set retransmission variable
 *     (c) If new ACK received and server is in fast recovery state:
 *             cwnd = ssthresh
 *             # server exit fast recovery state and goes into congestion
 *               avoidance state, ca counter is set to 0
 *     (d) If new ACK received and server is not in fast recovery state:
 *             # server goes into slow start or congestion avoidance
 *               depending on the relationship between cwnd and ssthresh
 *
 *  Return the min value of cwnd and awnd
 * --------------------------------------------------------------------------
 */
uint16_t cc_ack(uint32_t seq, uint16_t wnd) {
    this_ack = seq;
    awnd = wnd;
    if (this_ack == last_ack)
        dup_c ++;
    else
        dup_c = 0; // TODO: New ACK, sliding window

    printf("[Server Child #%d]: CC ACK. (ACK = %d, awnd = %d, dup_c = %d)\n", pid, seq, wnd, dup_c);

    if (dup_c > 3 && fast_rec == 1) {
        // fast recovery
        cwnd += 1;
        printf("[Server Child #%d]: CC Fast Recovery (Duplicate ACK received) (cwnd = %d)\n", pid, cwnd);
    } else if (dup_c == 3) {
        // fast retransmit
        ssthresh = cwnd >> 1;
        // cwnd = ssthresh + 3;
        // TODO: retransmit
        fast_rec = 1;
        printf("[Server Child #%d]: CC Fast Retransmit and Fast Recovery triggered. (cwnd = %d, ssthresh = %d)\n", pid, cwnd, ssthresh);
    } else if (dup_c == 0 && fast_rec == 1) {
        // state: congestion avoidance
        cwnd = ssthresh;
        fast_rec = 0;
        ca_c = 0;
        printf("[Server Child #%d]: CC Fast Recovery (New ACK received) (cwnd = %d, ssthresh = %d)\n", pid, cwnd, ssthresh);
    } else if (dup_c == 0) {
        if (cwnd < ssthresh)
            slow_start();
        else
            congestion_avoidance();
    }

    // TODO: use ts to update RCC and RTO
    last_ack = this_ack;

    return min(cwnd, awnd);
}

