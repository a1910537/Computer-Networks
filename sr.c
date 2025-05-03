#include <stddef.h>  // 为了使用 NULL
#include "sr.h"

#define SEQSPACE (2 * WINDOW_SIZE)

static struct pkt A_buffer[SEQSPACE];
static int A_ack[SEQSPACE];
static int A_base = 0;
static int A_nextseq = 0;

static struct pkt B_buffer[SEQSPACE];
static int B_received[SEQSPACE];
static int B_expected = 0;

int compute_checksum(struct pkt *packet) {
    int checksum = packet->seqnum + packet->acknum;
    int i;
    for (i = 0; i < 20; i++) {
        checksum += packet->payload[i];
    }
    return checksum;
}

struct pkt make_pkt(int seq, int ack, const char *data) {
    struct pkt packet;
    int i;
    packet.seqnum = seq;
    packet.acknum = ack;
    for (i = 0; i < 20; i++) {
        packet.payload[i] = data ? data[i] : 0;
    }
    packet.checksum = compute_checksum(&packet);
    return packet;
}

void send_window() {
    int i;
    for (i = 0; i < WINDOW_SIZE; i++) {
        int idx = (A_base + i) % SEQSPACE;
        if (!A_ack[idx] && A_buffer[idx].seqnum >= 0) {
            tolayer3(0, A_buffer[idx]);
        }
    }
    starttimer(0, TIMEOUT);
}

void A_init() {
    int i;
    for (i = 0; i < SEQSPACE; i++) {
        A_buffer[i].seqnum = -1;
        A_ack[i] = 0;
    }
    A_base = 0;
    A_nextseq = 0;
}

void A_output(struct msg message) {
    if ((A_nextseq + SEQSPACE - A_base) % SEQSPACE >= WINDOW_SIZE) {
        return;
    }

    struct pkt packet = make_pkt(A_nextseq, -1, message.data);
    int idx = A_nextseq % SEQSPACE;
    A_buffer[idx] = packet;
    A_ack[idx] = 0;

    tolayer3(0, packet);
    if (A_base == A_nextseq) {
        starttimer(0, TIMEOUT);
    }

    A_nextseq = (A_nextseq + 1) % SEQSPACE;
}

void A_input(struct pkt packet) {
    int checksum = compute_checksum(&packet);
    if (checksum != packet.checksum || packet.acknum < 0) return;

    int idx = packet.acknum % SEQSPACE;
    if (!A_ack[idx]) {
        A_ack[idx] = 1;
        while (A_ack[A_base % SEQSPACE]) {
            A_ack[A_base % SEQSPACE] = 0;
            A_buffer[A_base % SEQSPACE].seqnum = -1;
            A_base = (A_base + 1) % SEQSPACE;
        }
        stoptimer(0);
        if (A_base != A_nextseq) {
            starttimer(0, TIMEOUT);
        }
    }
}

void A_timerinterrupt() {
    send_window();
}

void B_init() {
    int i;
    for (i = 0; i < SEQSPACE; i++) {
        B_received[i] = 0;
        B_buffer[i].seqnum = -1;
    }
    B_expected = 0;
}

void B_input(struct pkt packet) {
    int checksum = compute_checksum(&packet);
    if (checksum != packet.checksum) return;

    int seq = packet.seqnum;
    int idx = seq % SEQSPACE;

    if ((seq >= B_expected && seq < B_expected + WINDOW_SIZE) ||
        (B_expected + WINDOW_SIZE >= SEQSPACE && seq < (B_expected + WINDOW_SIZE) % SEQSPACE)) {
        if (!B_received[idx]) {
            B_received[idx] = 1;
            B_buffer[idx] = packet;
        }

        while (B_received[B_expected % SEQSPACE]) {
            tolayer5(1, B_buffer[B_expected % SEQSPACE].payload);
            B_received[B_expected % SEQSPACE] = 0;
            B_expected = (B_expected + 1) % SEQSPACE;
        }
    }

    struct pkt ack = make_pkt(0, seq, NULL);
    tolayer3(1, ack);
}
