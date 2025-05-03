#include "sr.h"

static struct pkt A_buffer[WINDOW_SIZE];
static int A_ack[WINDOW_SIZE];
static int A_base = 0;
static int A_nextseq = 0;

static struct pkt B_buffer[WINDOW_SIZE];
static int B_received[WINDOW_SIZE];
static int B_expected = 0;

int compute_checksum(struct pkt *packet) {
    int checksum = packet->seqnum + packet->acknum;
    int i;
    for (i = 0; i < 20; i++) {
        checksum += packet->payload[i];
    }
    return checksum;
}

struct pkt make_pkt(int seq, int ack, char *data) {
    struct pkt packet;
    packet.seqnum = seq;
    packet.acknum = ack;
    int i;
    for (i = 0; i < 20; i++) {
        packet.payload[i] = data ? data[i] : 0;
    }
    packet.checksum = compute_checksum(&packet);
    return packet;
}

void send_window() {
    int i;
    for (i = 0; i < WINDOW_SIZE; i++) {
        int idx = (A_base + i) % WINDOW_SIZE;
        if (!A_ack[idx] && A_buffer[idx].seqnum >= 0) {
            tolayer3(0, A_buffer[idx]);
        }
    }
    starttimer(0, TIMEOUT);
}

void A_init() {
    int i;
    for (i = 0; i < WINDOW_SIZE; i++) {
        A_buffer[i].seqnum = -1;
        A_ack[i] = 0;
    }
    A_base = 0;
    A_nextseq = 0;
}

void A_output(struct msg message) {
    if ((A_nextseq + WINDOW_SIZE - A_base) % WINDOW_SIZE >= WINDOW_SIZE) {
        return;
    }

    struct pkt packet = make_pkt(A_nextseq, -1, message.data);
    int idx = A_nextseq % WINDOW_SIZE;
    A_buffer[idx] = packet;
    A_ack[idx] = 0;

    tolayer3(0, packet);
    if (A_base == A_nextseq) {
        starttimer(0, TIMEOUT);
    }

    A_nextseq = (A_nextseq + 1) % (WINDOW_SIZE * 2);
}

void A_input(struct pkt packet) {
    int checksum = compute_checksum(&packet);
    if (checksum != packet.checksum || packet.acknum < 0) return;

    int idx = packet.acknum % WINDOW_SIZE;
    if (!A_ack[idx]) {
        A_ack[idx] = 1;
        while (A_ack[A_base % WINDOW_SIZE]) {
            A_ack[A_base % WINDOW_SIZE] = 0;
            A_buffer[A_base % WINDOW_SIZE].seqnum = -1;
            A_base = (A_base + 1) % (WINDOW_SIZE * 2);
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
    for (i = 0; i < WINDOW_SIZE; i++) {
        B_received[i] = 0;
        B_buffer[i].seqnum = -1;
    }
    B_expected = 0;
}

void B_input(struct pkt packet) {
    int checksum = compute_checksum(&packet);
    if (checksum != packet.checksum) return;

    int seq = packet.seqnum;
    int idx = seq % WINDOW_SIZE;

    if (seq >= B_expected && seq < B_expected + WINDOW_SIZE) {
        if (!B_received[idx]) {
            B_received[idx] = 1;
            B_buffer[idx] = packet;
        }

        while (B_received[B_expected % WINDOW_SIZE]) {
            tolayer5(1, B_buffer[B_expected % WINDOW_SIZE].payload);
            B_received[B_expected % WINDOW_SIZE] = 0;
            B_expected = (B_expected + 1) % (WINDOW_SIZE * 2);
        }
    }

    struct pkt ack = make_pkt(0, seq, NULL);
    tolayer3(1, ack);
}
