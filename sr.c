#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "sr.h"

#define WINDOW_SIZE 8
#define SEQSPACE 16
#define TIMEOUT 20.0

struct pkt A_buffer[SEQSPACE];
int A_ack[SEQSPACE];
int A_base = 0;
int A_nextseq = 0;

struct pkt B_buffer[SEQSPACE];
int B_received[SEQSPACE];
int B_expected = 0;

int compute_checksum(struct pkt packet) {
    int sum = packet.seqnum + packet.acknum;
    for (int i = 0; i < 20; i++)
        sum += (unsigned char)packet.payload[i];
    return sum;
}

struct pkt make_pkt(int seq, int ack, char *data) {
    struct pkt packet;
    packet.seqnum = seq;
    packet.acknum = ack;
    memset(packet.payload, 0, 20);
    if (data)
        strncpy(packet.payload, data, 20);
    packet.checksum = compute_checksum(packet);
    return packet;
}

void A_output(struct msg message) {
    if ((A_nextseq + SEQSPACE - A_base) % SEQSPACE < WINDOW_SIZE) {
        struct pkt packet = make_pkt(A_nextseq, -1, message.data);
        A_buffer[A_nextseq] = packet;
        A_ack[A_nextseq] = 0;
        tolayer3(0, packet);
        starttimer(0, TIMEOUT);
        A_nextseq = (A_nextseq + 1) % SEQSPACE;
    }
}

void A_input(struct pkt packet) {
    if (compute_checksum(packet) == packet.checksum) {
        int idx = packet.acknum % SEQSPACE;
        if (!A_ack[idx]) {
            A_ack[idx] = 1;
            if (idx == A_base) {
                while (A_ack[A_base]) {
                    stoptimer(0);
                    A_ack[A_base] = 0;
                    A_base = (A_base + 1) % SEQSPACE;
                }
                if (A_base != A_nextseq)
                    starttimer(0, TIMEOUT);
            }
        }
    }
}

void A_timerinterrupt() {
    for (int i = 0; i < SEQSPACE; i++) {
        if ((A_base + i) % SEQSPACE != A_nextseq && !A_ack[(A_base + i) % SEQSPACE]) {
            tolayer3(0, A_buffer[(A_base + i) % SEQSPACE]);
        }
    }
    starttimer(0, TIMEOUT);
}

void A_init() {
    A_base = 0;
    A_nextseq = 0;
    for (int i = 0; i < SEQSPACE; i++) {
        A_ack[i] = 0;
    }
}

void B_input(struct pkt packet) {
    if (compute_checksum(packet) != packet.checksum)
        return;
    int seq = packet.seqnum;
    if ((seq >= B_expected && seq < B_expected + WINDOW_SIZE) || 
        (B_expected + WINDOW_SIZE >= SEQSPACE && seq < (B_expected + WINDOW_SIZE) % SEQSPACE)) {
        if (!B_received[seq]) {
            B_buffer[seq] = packet;
            B_received[seq] = 1;
        }
        while (B_received[B_expected]) {
            tolayer5(1, B_buffer[B_expected].payload);
            B_received[B_expected] = 0;
            B_expected = (B_expected + 1) % SEQSPACE;
        }
    }
    struct pkt ack = make_pkt(0, seq, NULL);
    tolayer3(1, ack);
}

void B_init() {
    B_expected = 0;
    for (int i = 0; i < SEQSPACE; i++) {
        B_received[i] = 0;
    }
}
