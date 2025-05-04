#include "sr.h"
#include "emulator.h"
#include <string.h>
#include <stddef.h>

#define WINDOW_SIZE 8
#define SEQSPACE (2 * WINDOW_SIZE)
#define TIMEOUT 20.0

static struct pkt A_buffer[SEQSPACE];
static int A_acknowledged[SEQSPACE];
static int A_base = 0;
static int A_nextseq = 0;

static struct pkt B_buffer[SEQSPACE];
static int B_received[SEQSPACE];
static int B_expected = 0;

int compute_checksum(struct pkt *p) {
    int checksum = 0;
    int i;
    checksum += p->seqnum + p->acknum;
    for (i = 0; i < 20; ++i)
        checksum += p->payload[i];
    return checksum;
}

struct pkt make_pkt(int seq, int ack, const char *data) {
    struct pkt p;
    p.seqnum = seq;
    p.acknum = ack;
    memset(p.payload, 0, sizeof(p.payload));
    if (data != NULL)
        strncpy(p.payload, data, sizeof(p.payload));
    p.checksum = compute_checksum(&p);
    return p;
}

void A_output(struct msg message) {
    if ((A_nextseq - A_base + SEQSPACE) % SEQSPACE >= WINDOW_SIZE)
        return; // Window full

    struct pkt p = make_pkt(A_nextseq, -1, message.data);
    A_buffer[A_nextseq] = p;
    A_acknowledged[A_nextseq] = 0;

    tolayer3(0, p);
    if (A_base == A_nextseq)
        starttimer(0, TIMEOUT);
    A_nextseq = (A_nextseq + 1) % SEQSPACE;
}

void A_input(struct pkt packet) {
    int checksum = compute_checksum(&packet);
    if (checksum != packet.checksum) return;

    int ack = packet.acknum;
    if (((ack - A_base + SEQSPACE) % SEQSPACE) < WINDOW_SIZE && !A_acknowledged[ack]) {
        A_acknowledged[ack] = 1;
        while (A_acknowledged[A_base]) {
            A_acknowledged[A_base] = 0;
            A_base = (A_base + 1) % SEQSPACE;
        }
        if (A_base == A_nextseq)
            stoptimer(0);
        else
            starttimer(0, TIMEOUT);
    }
}

void A_timerinterrupt() {
    int i;
    for (i = 0; i < WINDOW_SIZE; ++i) {
        int idx = (A_base + i) % SEQSPACE;
        if (!A_acknowledged[idx])
            tolayer3(0, A_buffer[idx]);
    }
    starttimer(0, TIMEOUT);
}

void A_init() {
    int i;
    for (i = 0; i < SEQSPACE; ++i)
        A_acknowledged[i] = 0;
    A_base = 0;
    A_nextseq = 0;
}

void B_input(struct pkt packet) {
    int checksum = compute_checksum(&packet);
    if (checksum != packet.checksum)
        return;

    int seq = packet.seqnum;
    struct pkt ack = make_pkt(0, seq, NULL);
    tolayer3(1, ack);

    if (!B_received[seq]) {
        B_buffer[seq] = packet;
        B_received[seq] = 1;

        while (B_received[B_expected]) {
            tolayer5(1, B_buffer[B_expected].payload);
            B_received[B_expected] = 0;
            B_expected = (B_expected + 1) % SEQSPACE;
        }
    }
}

void B_init() {
    int i;
    for (i = 0; i < SEQSPACE; ++i)
        B_received[i] = 0;
    B_expected = 0;
}