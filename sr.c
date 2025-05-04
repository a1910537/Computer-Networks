#include <stdio.h>
#include "sr.h"
#include "emulator.h"

#define RTT 16.0
#define WINDOWSIZE 6
#define SEQSPACE 12
#define NOTINUSE (-1)

static struct pkt buffer[SEQSPACE];
static int acked[SEQSPACE];
static int used[SEQSPACE];
static int base;
static int nextseqnum;

int ComputeChecksum(struct pkt packet) {
  int checksum = 0;
  int i;
  checksum += packet.seqnum + packet.acknum;
  for (i = 0; i < 20; i++) {
    checksum += packet.payload[i];
  }
  return checksum;
}

int IsCorrupted(struct pkt packet) {
  return packet.checksum != ComputeChecksum(packet);
}

void A_init(void) {
  int i;
  base = 0;
  nextseqnum = 0;
  for (i = 0; i < SEQSPACE; i++) {
    acked[i] = 0;
    used[i] = 0;
  }
}

void A_output(struct msg message) {
  if ((nextseqnum - base + SEQSPACE) % SEQSPACE < WINDOWSIZE) {
    struct pkt p;
    int i;
    p.seqnum = nextseqnum;
    p.acknum = NOTINUSE;
    for (i = 0; i < 20; i++) {
      p.payload[i] = message.data[i];
    }
    p.checksum = ComputeChecksum(p);
    buffer[nextseqnum] = p;
    used[nextseqnum] = 1;
    acked[nextseqnum] = 0;
    tolayer3(0, p);
    if (base == nextseqnum) {
      starttimer(0, RTT);
    }
    nextseqnum = (nextseqnum + 1) % SEQSPACE;
  } else {
    window_full++;
  }
}

void A_input(struct pkt packet) {
  int ack = packet.acknum;
  if (!IsCorrupted(packet) && used[ack]) {
    total_ACKs_received++;
    if (!acked[ack]) {
      new_ACKs++;
      acked[ack] = 1;
      if (ack == base) {
        stoptimer(0);
        while (acked[base]) {
          acked[base] = 0;
          used[base] = 0;
          base = (base + 1) % SEQSPACE;
        }
        if (base != nextseqnum) {
          starttimer(0, RTT);
        }
      }
    }
  }
}

void A_timerinterrupt(void) {
  int i;
  starttimer(0, RTT);
  for (i = 0; i < SEQSPACE; i++) {
    if (used[i] && !acked[i]) {
      tolayer3(0, buffer[i]);
      packets_resent++;
    }
  }
}

/* ------------------------ B SIDE ------------------------- */

static struct pkt recv_buf[SEQSPACE];
static int received[SEQSPACE];
static int expected_base;

void B_init(void) {
  int i;
  expected_base = 0;
  for (i = 0; i < SEQSPACE; i++) {
    received[i] = 0;
  }
}

void B_input(struct pkt packet) {
  int seq = packet.seqnum;
  struct pkt ackpkt;
  int i;
  int in_window = 0;
  int window_end = (expected_base + WINDOWSIZE) % SEQSPACE;

  if (!IsCorrupted(packet)) {
    packets_received++;
    if (expected_base <= window_end) {
      if (seq >= expected_base && seq < window_end) {
        in_window = 1;
      }
    } else {
      if (seq >= expected_base || seq < window_end) {
        in_window = 1;
      }
    }
    if (in_window && !received[seq]) {
      recv_buf[seq] = packet;
      received[seq] = 1;
      while (received[expected_base]) {
        tolayer5(1, recv_buf[expected_base].payload);
        received[expected_base] = 0;
        expected_base = (expected_base + 1) % SEQSPACE;
      }
    }
    ackpkt.acknum = seq;
  } else {
    ackpkt.acknum = (expected_base + SEQSPACE - 1) % SEQSPACE;
  }

  ackpkt.seqnum = 0;
  for (i = 0; i < 20; i++) {
    ackpkt.payload[i] = 0;
  }
  ackpkt.checksum = ComputeChecksum(ackpkt);
  tolayer3(1, ackpkt);
}

void B_output(struct msg message) {}
void B_timerinterrupt(void) {}
