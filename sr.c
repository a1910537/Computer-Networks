#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "emulator.h"
#include "sr.h"

#define TIMEOUT 20.0

struct pkt make_pkt(int seqnum, int acknum, char *data);
bool is_corrupted(struct pkt packet);
int compute_checksum(struct pkt packet);
void tolayer3(int AorB, struct pkt packet);
void tolayer5(int AorB, char datasent[20]);
void starttimer(int AorB, float increment);
void stoptimer(int AorB);

// A-side buffers and state
static int A_base = 0;
static int A_nextseq = 0;
static struct pkt A_buffer[SEQSPACE];
static bool A_ack[SEQSPACE];
static struct msg A_msg_buffer[1000];
static int A_msg_count = 0;
static int A_msg_index = 0;
static bool A_timer_running = false;

// B-side buffers and state
static int B_expected = 0;
static struct pkt B_buffer[SEQSPACE];
static bool B_received[SEQSPACE];

struct pkt make_pkt(int seqnum, int acknum, char *data) {
    struct pkt packet;
    packet.seqnum = seqnum;
    packet.acknum = acknum;
    memset(packet.payload, 0, 20);
    if (data != NULL)
        memcpy(packet.payload, data, strlen(data));
    packet.checksum = compute_checksum(packet);
    return packet;
}

int compute_checksum(struct pkt packet) {
    int sum = packet.seqnum + packet.acknum;
    for (int i = 0; i < 20; i++)
        sum += packet.payload[i];
    return sum;
}

bool is_corrupted(struct pkt packet) {
    return packet.checksum != compute_checksum(packet);
}

// helper: seqnum in window
bool in_window(int base, int seqnum) {
    if (base <= seqnum)
        return seqnum < base + WINDOW_SIZE;
    return seqnum < (base + WINDOW_SIZE) % SEQSPACE || seqnum >= base;
}

void A_output(struct msg message) {
    if ((A_nextseq + SEQSPACE - A_base) % SEQSPACE >= WINDOW_SIZE) {
        // window full, buffer message
        if (A_msg_count < 1000)
            A_msg_buffer[A_msg_count++] = message;
        return;
    }

    struct pkt packet = make_pkt(A_nextseq, -1, message.data);
    A_buffer[A_nextseq] = packet;
    A_ack[A_nextseq] = false;

    tolayer3(0, packet);
    printf("Sending packet %d to layer 3\n", A_nextseq);

    if (!A_timer_running) {
        starttimer(0, TIMEOUT);
        A_timer_running = true;
    }

    A_nextseq = (A_nextseq + 1) % SEQSPACE;
}

void A_input(struct pkt packet) {
    if (is_corrupted(packet) || !in_window(A_base, packet.acknum))
        return;

    A_ack[packet.acknum] = true;
    while (A_ack[A_base]) {
        A_ack[A_base] = false;
        A_base = (A_base + 1) % SEQSPACE;
    }

    if (A_base == A_nextseq) {
        stoptimer(0);
        A_timer_running = false;
    } else {
        stoptimer(0);
        starttimer(0, TIMEOUT);
        A_timer_running = true;
    }

    while ((A_nextseq + SEQSPACE - A_base) % SEQSPACE < WINDOW_SIZE && A_msg_index < A_msg_count) {
        A_output(A_msg_buffer[A_msg_index++]);
    }
}

void A_timerinterrupt() {
    starttimer(0, TIMEOUT);
    A_timer_running = true;

    for (int i = 0; i < WINDOW_SIZE; i++) {
        int idx = (A_base + i) % SEQSPACE;
        if ((A_nextseq + SEQSPACE - A_base) % SEQSPACE > i && !A_ack[idx]) {
            tolayer3(0, A_buffer[idx]);
            printf("---A: retransmitting packet %d\n", idx);
        }
    }
}

void A_init() {
    A_base = 0;
    A_nextseq = 0;
    A_timer_running = false;
    memset(A_ack, 0, sizeof(A_ack));
    A_msg_count = 0;
    A_msg_index = 0;
}

// B side
void B_input(struct pkt packet) {
    if (is_corrupted(packet))
        return;

    int seq = packet.seqnum;
    if (!in_window(B_expected, seq)) {
        struct pkt ack = make_pkt(0, seq, NULL);
        tolayer3(1, ack);
        return;
    }

    if (!B_received[seq]) {
        B_received[seq] = true;
        B_buffer[seq] = packet;
    }

    struct pkt ack = make_pkt(0, seq, NULL);
    tolayer3(1, ack);

    while (B_received[B_expected]) {
        tolayer5(1, B_buffer[B_expected].payload);
        B_received[B_expected] = false;
        B_expected = (B_expected + 1) % SEQSPACE;
    }
}

void B_init() {
    B_expected = 0;
    memset(B_received, 0, sizeof(B_received));
}
