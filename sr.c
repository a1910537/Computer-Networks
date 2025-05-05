/*In the expected behavior of test 4, it seems that "3 messages are sent" instead of a single
   packet. This conflicts with "single packet sent without packet loss". Looking at the diff,
    it seems that stat check 4 and full trace 4 are different ways of describing the same 
    test, but the descriptions about the number of messages are inconsistent. Finally, 
    it was found that error messages related to "packet corruption or unexpected sequence 
    number" appeared in multiple places, which may be the reason for the difference in 
    the code.
*/
/*
  Refactored Selective Repeat with C90 compatibility,
  original trace strings preserved,
  corrupted packets/ACKs dropped silently.
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "emulator.h"
#include "sr.h"

#define TOUT_INTERVAL 16.0
#define MAX_WINDOW    6
#define SEQ_MODULUS   12
#define NO_ACK        (-1)

static struct pkt pkt_buffer[SEQ_MODULUS];
static bool is_bufd[SEQ_MODULUS];
static bool is_ackd[SEQ_MODULUS];
static int win_start;
static int next_seq;

static struct pkt rcv_buffer[SEQ_MODULUS];
static bool rcvd[SEQ_MODULUS];
static int expect_id;
static int b_toggle;

static int calc_crc(const struct pkt p)
{
    int sum;
    int idx;
    sum = p.seqnum + p.acknum;
    for (idx = 0; idx < 20; idx++)
        sum += (unsigned char)p.payload[idx];
    return sum;
}

static bool pkt_is_corrupt(const struct pkt p)
{
    return (p.checksum != calc_crc(p));
}

void A_init(void)
{
    int idx;
    win_start = 0;
    next_seq  = 0;
    for (idx = 0; idx < SEQ_MODULUS; idx++)
    {
        is_bufd[idx] = false;
        is_ackd[idx] = false;
    }
}

void A_output(struct msg app_msg)
{
    int win_size;
    int idx;
    struct pkt newp;

    win_size = (next_seq - win_start + SEQ_MODULUS) % SEQ_MODULUS;
    if (win_size < MAX_WINDOW)
    {
        if (TRACE > 1)
            printf("----A: New message arrives, send window is not full, send new messge to layer3!\n");

        newp.seqnum  = next_seq;
        newp.acknum  = NO_ACK;
        for (idx = 0; idx < 20; idx++)
            newp.payload[idx] = app_msg.data[idx];
        newp.checksum = calc_crc(newp);

        pkt_buffer[next_seq] = newp;
        is_bufd[next_seq]    = true;
        is_ackd[next_seq]    = false;

        if (TRACE > 0)
            printf("Sending packet %d to layer 3\n", newp.seqnum);
        tolayer3(A, newp);

        if (win_start == next_seq)
            starttimer(A, TOUT_INTERVAL);

        next_seq = (next_seq + 1) % SEQ_MODULUS;
    }
    else
    {
        if (TRACE > 0)
            printf("----A: New message arrives, send window is full\n");
        window_full++;
    }
}

void A_input(struct pkt ack_pkt)
{
    int ackno;
    ackno = ack_pkt.acknum;
    if (pkt_is_corrupt(ack_pkt) || !is_bufd[ackno])
        return;

    if (TRACE > 0)
        printf("----A: uncorrupted ACK %d is received\n", ackno);
    total_ACKs_received++;

    if (!is_ackd[ackno])
    {
        if (TRACE > 0)
            printf("----A: ACK %d is not a duplicate\n", ackno);
        new_ACKs++;
        is_ackd[ackno] = true;

        if (ackno == win_start)
        {
            stoptimer(A);
            while (is_ackd[win_start])
            {
                is_bufd[win_start] = false;
                is_ackd[win_start] = false;
                win_start = (win_start + 1) % SEQ_MODULUS;
            }
            if (win_start != next_seq)
                starttimer(A, TOUT_INTERVAL);
        }
    }
    else if (TRACE > 0)
        printf("----A: duplicate ACK received, do nothing!\n");
}

void A_timerinterrupt(void)
{
    if (TRACE > 0)
        printf("----A: time out,resend packets!\n");

    if (is_bufd[win_start] && !is_ackd[win_start])
    {
        if (TRACE > 0)
            printf("---A: resending packet %d\n", pkt_buffer[win_start].seqnum);
        tolayer3(A, pkt_buffer[win_start]);
        packets_resent++;
    }
    starttimer(A, TOUT_INTERVAL);
}

void B_init(void)
{
    int idx;
    expect_id = 0;
    b_toggle  = 1;
    for (idx = 0; idx < SEQ_MODULUS; idx++)
        rcvd[idx] = false;
}

void B_input(struct pkt data_pkt)
{
    int sn;
    int win_end;
    int idx;
    bool in_win;

    if (pkt_is_corrupt(data_pkt))
        return;

    sn      = data_pkt.seqnum;
    win_end = (expect_id + MAX_WINDOW) % SEQ_MODULUS;
    if (expect_id <= win_end)
        in_win = (sn >= expect_id && sn < win_end);
    else
        in_win = (sn >= expect_id || sn < win_end);

    packets_received++;
    if (in_win)
    {
        if (TRACE > 0)
            printf("----B: packet %d is correctly received, send ACK!\n", sn);
        if (!rcvd[sn])
        {
            rcv_buffer[sn] = data_pkt;
            rcvd[sn]       = true;
            if (sn == expect_id)
            {
                while (rcvd[expect_id])
                {
                    tolayer5(B, rcv_buffer[expect_id].payload);
                    rcvd[expect_id] = false;
                    expect_id = (expect_id + 1) % SEQ_MODULUS;
                }
            }
        }
    }
    else
        return;

    /* send ACK for correctly received packet */
    {
        struct pkt ackp;
        ackp.seqnum  = b_toggle;
        ackp.acknum  = sn;
        for (idx = 0; idx < 20; idx++)
            ackp.payload[idx] = '0';
        ackp.checksum = calc_crc(ackp);
        tolayer3(B, ackp);
        b_toggle = (b_toggle + 1) % 2;
    }
}

void B_output(struct msg m) {}
void B_timerinterrupt(void) {}
