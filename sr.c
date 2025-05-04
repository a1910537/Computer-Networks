#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "emulator.h"
#include "sr.h"

/***************************************************************
 * Selective Repeat Protocol Implementation (English Version)
 *
 * Based on J.F. Kurose's ABT/GBN simulator, customized for SR
 *
 * - Unidirectional data flow (A to B)
 * - Network delay and packet loss/corruption are simulated
 * - Window-based retransmission with selective acknowledgement
 ***************************************************************/

#define RTT  16.0        // Round trip time for timers (set by assignment spec)
#define WINDOW_SIZE 6    // Sender window size
#define SEQ_SPACE 12     // Sequence number space (at least 2 * WINDOW_SIZE)
#define INVALID_SEQ -1   // Marker for unused sequence numbers

// Compute checksum: Used to detect corruption
int compute_checksum(struct pkt packet) {
    int checksum = packet.seqnum + packet.acknum;
    for (int i = 0; i < 20; ++i)
        checksum += (int)packet.payload[i];
    return checksum;
}

// Check if packet is corrupted
bool is_corrupted(struct pkt packet) {
    return compute_checksum(packet) != packet.checksum;
}

/************** Sender A-side **************/

static struct pkt send_buffer[SEQ_SPACE];  // Buffer of sent but unacked packets
static bool acked[SEQ_SPACE];              // ACK status for each sequence number
static bool valid[SEQ_SPACE];              // Indicates if buffer entry is in use
static int base = 0;                       // Oldest unacked sequence number
static int next_seq = 0;                   // Next sequence number to send

void A_output(struct msg message) {
    // If send window is full, ignore new message
    if ((next_seq - base + SEQ_SPACE) % SEQ_SPACE >= WINDOW_SIZE) {
        if (TRACE > 0) printf("[A] Send window full. Dropping message.\n");
        window_full++;
        return;
    }

    // Build packet
    struct pkt packet;
    packet.seqnum = next_seq;
    packet.acknum = INVALID_SEQ;
    for (int i = 0; i < 20; ++i)
        packet.payload[i] = message.data[i];
    packet.checksum = compute_checksum(packet);

    // Save to buffer and mark valid
    send_buffer[next_seq] = packet;
    valid[next_seq] = true;
    acked[next_seq] = false;

    // Send to layer 3
    if (TRACE > 0) printf("[A] Sending packet %d\n", packet.seqnum);
    tolayer3(A, packet);

    // Start timer for base
    if (base == next_seq)
        starttimer(A, RTT);

    next_seq = (next_seq + 1) % SEQ_SPACE;
}

void A_input(struct pkt packet) {
    int ack = packet.acknum;

    if (is_corrupted(packet) || !valid[ack]) {
        if (TRACE > 0) printf("[A] Corrupted or unknown ACK %d\n", ack);
        return;
    }

    if (!acked[ack]) {
        acked[ack] = true;
        total_ACKs_received++;
        new_ACKs++;

        if (TRACE > 0) printf("[A] Received new ACK for %d\n", ack);

        // Slide window if this is the base
        if (ack == base) {
            stoptimer(A);
            while (acked[base]) {
                valid[base] = false;
                acked[base] = false;
                base = (base + 1) % SEQ_SPACE;
            }
            if (base != next_seq)
                starttimer(A, RTT);
        }
    } else {
        if (TRACE > 0) printf("[A] Duplicate ACK %d\n", ack);
    }
}

void A_timerinterrupt(void) {
    if (valid[base] && !acked[base]) {
        if (TRACE > 0) printf("[A] Timeout for %d. Retransmitting.\n", base);
        tolayer3(A, send_buffer[base]);
        packets_resent++;
    }
    starttimer(A, RTT);
}

void A_init(void) {
    for (int i = 0; i < SEQ_SPACE; ++i) {
        acked[i] = false;
        valid[i] = false;
    }
    base = 0;
    next_seq = 0;
}

/************** Receiver B-side **************/

static struct pkt recv_buffer[SEQ_SPACE];  // Buffer for out-of-order reception
static bool received[SEQ_SPACE];          // Track which sequence numbers are filled
static int expect_seq = 0;                // Lower bound of B's receiving window
static int next_ack_seq = 1;              // Alternating seqnum for outgoing packets (not essential)

void B_input(struct pkt packet) {
    int seq = packet.seqnum;
    int window_end = (expect_seq + WINDOW_SIZE) % SEQ_SPACE;
    bool within_window = (expect_seq <= window_end) ?
        (seq >= expect_seq && seq < window_end) :
        (seq >= expect_seq || seq < window_end);

    if (!is_corrupted(packet)) {
        packets_received++;

        if (within_window) {
            if (!received[seq]) {
                recv_buffer[seq] = packet;
                received[seq] = true;
                if (TRACE > 0) printf("[B] Received expected packet %d\n", seq);

                while (received[expect_seq]) {
                    tolayer5(B, recv_buffer[expect_seq].payload);
                    received[expect_seq] = false;
                    expect_seq = (expect_seq + 1) % SEQ_SPACE;
                }
            }
        }

        // Always send ACK
        struct pkt ackpkt;
        ackpkt.seqnum = next_ack_seq;
        ackpkt.acknum = seq;
        for (int i = 0; i < 20; ++i)
            ackpkt.payload[i] = '0';
        ackpkt.checksum = compute_checksum(ackpkt);

        tolayer3(B, ackpkt);
        next_ack_seq = (next_ack_seq + 1) % 2;

    } else {
        if (TRACE > 0) printf("[B] Corrupted packet received. Sending last ACK.\n");
        struct pkt nack;
        nack.seqnum = next_ack_seq;
        nack.acknum = (expect_seq - 1 + SEQ_SPACE) % SEQ_SPACE;
        for (int i = 0; i < 20; ++i)
            nack.payload[i] = '0';
        nack.checksum = compute_checksum(nack);
        tolayer3(B, nack);
        next_ack_seq = (next_ack_seq + 1) % 2;
    }
}

void B_init(void) {
    for (int i = 0; i < SEQ_SPACE; ++i)
        received[i] = false;
    expect_seq = 0;
    next_ack_seq = 1;
}

/******** Optional bidirectional stubs ********/

void B_output(struct msg message) { }
void B_timerinterrupt(void) { }
