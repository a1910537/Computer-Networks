#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "emulator.h"
#include "sr.h"

#define RTT  16.0
#define WINDOWSIZE 6
#define SEQSPACE 12
#define NOTINUSE (-1)

/********* Common Utilities ************/
int ComputeChecksum(struct pkt packet) {
    int checksum = packet.seqnum + packet.acknum;
    for (int i = 0; i < 20; i++) {
        checksum += (int)(packet.payload[i]);
    }
    return checksum;
}

bool IsCorrupted(struct pkt packet) {
    return packet.checksum != ComputeChecksum(packet);
}

bool InWindow(int base, int seq) {
    return ((seq >= base && seq < base + WINDOWSIZE) || 
            (base + WINDOWSIZE >= SEQSPACE && seq < (base + WINDOWSIZE) % SEQSPACE));
}

/********* Sender (A) State ************/
static struct pkt send_buffer[SEQSPACE];
static bool acked[SEQSPACE];
static float timer_start[SEQSPACE];
static bool timer_active[SEQSPACE];
static int base_A;
static int nextseq_A;

void A_output(struct msg message) {
    if ((nextseq_A - base_A + SEQSPACE) % SEQSPACE < WINDOWSIZE) {
        struct pkt pkt;
        pkt.seqnum = nextseq_A;
        pkt.acknum = NOTINUSE;
        for (int i = 0; i < 20; i++) pkt.payload[i] = message.data[i];
        pkt.checksum = ComputeChecksum(pkt);
        send_buffer[nextseq_A] = pkt;
        acked[nextseq_A] = false;
        tolayer3(A, pkt);
        if (TRACE > 0) printf("A: Sent packet %d\n", pkt.seqnum);
        timer_start[nextseq_A] = time;
        timer_active[nextseq_A] = true;
        nextseq_A = (nextseq_A + 1) % SEQSPACE;
    } else {
        if (TRACE > 0) printf("A: Window full, message dropped\n");
        window_full++;
    }
}

void A_input(struct pkt packet) {
    if (!IsCorrupted(packet)) {
        int ack = packet.acknum;
        if (TRACE > 0) printf("A: Received ACK %d\n", ack);
        if (!acked[ack]) {
            acked[ack] = true;
            timer_active[ack] = false;
            new_ACKs++;
        }
    }
}

void A_timerinterrupt(void) {
    for (int i = 0; i < SEQSPACE; i++) {
        if (timer_active[i] && (time - timer_start[i]) >= RTT) {
            tolayer3(A, send_buffer[i]);
            timer_start[i] = time;
            packets_resent++;
            if (TRACE > 0) printf("A: Timeout, resend packet %d\n", i);
        }
    }
}

void A_init(void) {
    base_A = 0;
    nextseq_A = 0;
    for (int i = 0; i < SEQSPACE; i++) {
        acked[i] = false;
        timer_active[i] = false;
    }
}

/********* Receiver (B) State ************/
static struct pkt recv_buffer[SEQSPACE];
static bool received[SEQSPACE];
static int base_B;

void B_input(struct pkt packet) {
    struct pkt ackpkt;
    int seq = packet.seqnum;
    if (!IsCorrupted(packet) && InWindow(base_B, seq) && !received[seq]) {
        recv_buffer[seq] = packet;
        received[seq] = true;
        if (TRACE > 0) printf("B: Received packet %d\n", seq);
    } else {
        if (TRACE > 0) printf("B: Duplicate or out-of-window packet %d\n", seq);
    }

    ackpkt.seqnum = 0;
    ackpkt.acknum = seq;
    for (int i = 0; i < 20; i++) ackpkt.payload[i] = '0';
    ackpkt.checksum = ComputeChecksum(ackpkt);
    tolayer3(B, ackpkt);

    while (received[base_B]) {
        tolayer5(B, recv_buffer[base_B].payload);
        packets_received++;
        received[base_B] = false;
        base_B = (base_B + 1) % SEQSPACE;
    }
}

void B_init(void) {
    base_B = 0;
    for (int i = 0; i < SEQSPACE; i++) received[i] = false;
}

void B_output(struct msg message) {}
void B_timerinterrupt(void) {}
