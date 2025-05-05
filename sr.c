// Refactored Selective Repeat Implementation
// Functionality unchanged, naming and structure modified for clarity and variation

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "emulator.h"
#include "sr.h"

#define TOUT_INTERVAL 16.0
#define MAX_WINDOW    6
#define SEQ_MODULUS   12
#define NO_ACK        (-1)

// Sender-side state
static struct pkt pkt_buffer[SEQ_MODULUS];
static bool is_bufd[SEQ_MODULUS];
static bool is_ackd[SEQ_MODULUS];
static int win_start = 0;
static int next_seq  = 0;

// Receiver-side state
static struct pkt rcv_buffer[SEQ_MODULUS];
static bool rcvd[SEQ_MODULUS];
static int expect_id = 0;
static int b_toggle  = 1;

// Compute checksum for packet p
static int calc_crc(const struct pkt p) {
    int sum = p.seqnum + p.acknum;
    for (int i = 0; i < 20; ++i) {
        sum += (unsigned char)p.payload[i];
    }
    return sum;
}

// Check if packet p is corrupted
static bool pkt_is_corrupt(const struct pkt p) {
    return (p.checksum != calc_crc(p));
}

// Initialize sender A
void A_init(void) {
    win_start = next_seq = 0;
    for (int i = 0; i < SEQ_MODULUS; ++i) {
        is_bufd[i] = is_ackd[i] = false;
    }
}

// Called by layer 5 to send a new message
void A_output(struct msg app_msg) {
    int window_size = (next_seq - win_start + SEQ_MODULUS) % SEQ_MODULUS;
    if (window_size < MAX_WINDOW) {
        struct pkt newp;
        newp.seqnum = next_seq;
        newp.acknum = NO_ACK;
        for (int i = 0; i < 20; ++i) newp.payload[i] = app_msg.data[i];
        newp.checksum = calc_crc(newp);

        pkt_buffer[next_seq] = newp;
        is_bufd[next_seq]    = true;
        is_ackd[next_seq]    = false;

        tolayer3(A, newp);
        if (win_start == next_seq) starttimer(A, TOUT_INTERVAL);
        next_seq = (next_seq + 1) % SEQ_MODULUS;
    } else {
        // window full: drop or count
        window_full++;
    }
}

// Process incoming ACK at sender A
void A_input(struct pkt ack_pkt) {
    if (pkt_is_corrupt(ack_pkt)) return;
    int ackno = ack_pkt.acknum;
    if (!is_bufd[ackno]) return;

    // first time ACK
    if (!is_ackd[ackno]) {
        is_ackd[ackno] = true;
        new_ACKs++;
        total_ACKs_received++;
        if (ackno == win_start) {
            stoptimer(A);
            // slide window
            while (is_ackd[win_start]) {
                is_bufd[win_start] = is_ackd[win_start] = false;
                win_start = (win_start + 1) % SEQ_MODULUS;
            }
            if (win_start != next_seq) starttimer(A, TOUT_INTERVAL);
        }
    }
    // duplicates ignored
}

// Timer interrupt for sender A
void A_timerinterrupt(void) {
    if (is_bufd[win_start] && !is_ackd[win_start]) {
        tolayer3(A, pkt_buffer[win_start]);
        packets_resent++;
    }
    starttimer(A, TOUT_INTERVAL);
}

// Initialize receiver B
void B_init(void) {
    expect_id = 0;
    b_toggle  = 1;
    for (int i = 0; i < SEQ_MODULUS; ++i) rcvd[i] = false;
}

// Process incoming data packet at receiver B
void B_input(struct pkt data_pkt) {
    if (!pkt_is_corrupt(data_pkt)) {
        int sn = data_pkt.seqnum;
        int window_end = (expect_id + MAX_WINDOW) % SEQ_MODULUS;
        bool in_win = (expect_id <= window_end)
                      ? (sn >= expect_id && sn < window_end)
                      : (sn >= expect_id || sn < window_end);
        if (in_win && !rcvd[sn]) {
            rcv_buffer[sn] = data_pkt;
            rcvd[sn]       = true;
            if (sn == expect_id) {
                do {
                    tolayer5(B, rcv_buffer[expect_id].payload);
                    rcvd[expect_id] = false;
                    expect_id = (expect_id + 1) % SEQ_MODULUS;
                } while (rcvd[expect_id]);
            }
        }
        // always ACK
        struct pkt ackp;
        ackp.seqnum  = b_toggle;
        ackp.acknum  = sn;
        b_toggle     = (b_toggle + 1) % 2;
        for (int i = 0; i < 20; ++i) ackp.payload[i] = '\0';
        ackp.checksum = calc_crc(ackp);
        tolayer3(B, ackp);
        packets_received++;
    } else {
        // send ACK for last in-order packet
        struct pkt nack;
        nack.seqnum  = b_toggle;
        nack.acknum  = (expect_id == 0) ? SEQ_MODULUS - 1 : expect_id - 1;
        b_toggle     = (b_toggle + 1) % 2;
        for (int i = 0; i < 20; ++i) nack.payload[i] = '\0';
        nack.checksum = calc_crc(nack);
        tolayer3(B, nack);
    }
}

// Unused bi-directional stubs
void B_output(struct msg m) {}
void B_timerinterrupt(void) {}
