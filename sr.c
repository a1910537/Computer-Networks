#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "emulator.h"
#include "sr.h"

#define RTT  16.0
#define WINDOWSIZE 6
#define SEQSPACE 12
#define NOTINUSE (-1)

int ComputeChecksum(struct pkt packet)
{
  int checksum = packet.seqnum + packet.acknum;
  for (int i = 0; i < 20; i++)
    checksum += (int)(packet.payload[i]);
  return checksum;
}

bool IsCorrupted(struct pkt packet)
{
  return packet.checksum != ComputeChecksum(packet);
}

bool InWindow(int base, int seq)
{
  if (base + WINDOWSIZE < SEQSPACE)
    return seq >= base && seq < base + WINDOWSIZE;
  else
    return seq >= base || seq < (base + WINDOWSIZE) % SEQSPACE;
}

/********* Sender (A) ************/
static struct pkt buffer[SEQSPACE];
static bool acked[SEQSPACE];
static bool used[SEQSPACE];
static int base;
static int nextseqnum;

void A_output(struct msg message)
{
  struct pkt sendpkt;
  if ((nextseqnum - base + SEQSPACE) % SEQSPACE < WINDOWSIZE) {
    sendpkt.seqnum = nextseqnum;
    sendpkt.acknum = NOTINUSE;
    for (int i = 0; i < 20; i++)
      sendpkt.payload[i] = message.data[i];
    sendpkt.checksum = ComputeChecksum(sendpkt);

    buffer[nextseqnum] = sendpkt;
    used[nextseqnum] = true;
    acked[nextseqnum] = false;

    tolayer3(A, sendpkt);
    if (TRACE > 0) printf("Sending packet %d to layer 3\n", sendpkt.seqnum);

    if (base == nextseqnum)
      starttimer(A, RTT);

    nextseqnum = (nextseqnum + 1) % SEQSPACE;
  } else {
    if (TRACE > 0) printf("Window full, dropping message\n");
    window_full++;
  }
}

void A_input(struct pkt packet)
{
  int ack = packet.acknum;

  if (!IsCorrupted(packet) && InWindow(base, ack) && used[ack]) {
    if (TRACE > 0)
      printf("----A: uncorrupted ACK %d is received\n", ack);
    total_ACKs_received++;

    if (!acked[ack]) {
      new_ACKs++;
      acked[ack] = true;

      if (ack == base) {
        stoptimer(A);
        while (acked[base]) {
          acked[base] = false;
          used[base] = false;
          base = (base + 1) % SEQSPACE;
        }
        if (base != nextseqnum)
          starttimer(A, RTT);
      }
    } else {
      if (TRACE > 0)
        printf("----A: duplicate ACK received, do nothing!\n");
    }
  } else {
    if (TRACE > 0)
      printf("----A: corrupted or irrelevant ACK %d ignored\n", ack);
  }
}

void A_timerinterrupt(void)
{
  if (TRACE > 0) printf("Timeout occurred. Resending unacked packets.\n");
  int seq = base;
  for (int i = 0; i < WINDOWSIZE; i++) {
    if (used[seq] && !acked[seq]) {
      tolayer3(A, buffer[seq]);
      if (TRACE > 0) printf("Resending packet %d\n", seq);
      packets_resent++;
    }
    seq = (seq + 1) % SEQSPACE;
  }
  starttimer(A, RTT);
}

void A_init(void)
{
  base = nextseqnum = 0;
  for (int i = 0; i < SEQSPACE; i++) {
    used[i] = false;
    acked[i] = false;
  }
}

/********* Receiver (B) ************/
static struct pkt recv_buffer[SEQSPACE];
static bool received[SEQSPACE];
static int expected_base;
static int B_nextseqnum;

void B_input(struct pkt packet)
{
  struct pkt ackpkt;
  int seq = packet.seqnum;

  int window_end = (expected_base + WINDOWSIZE) % SEQSPACE;
  bool in_window = (expected_base <= window_end)
                   ? (seq >= expected_base && seq < window_end)
                   : (seq >= expected_base || seq < window_end);

  if (!IsCorrupted(packet)) {
    packets_received++;
    if (in_window) {
      if (!received[seq]) {
        recv_buffer[seq] = packet;
        received[seq] = true;

        if (seq == expected_base) {
          while (received[expected_base]) {
            tolayer5(B, recv_buffer[expected_base].payload);
            received[expected_base] = false;
            expected_base = (expected_base + 1) % SEQSPACE;
          }
        }
      }
    }

    ackpkt.acknum = seq;
  } else {
    ackpkt.acknum = (expected_base == 0) ? SEQSPACE - 1 : expected_base - 1;
  }

  ackpkt.seqnum = B_nextseqnum;
  B_nextseqnum = (B_nextseqnum + 1) % 2;
  for (int i = 0; i < 20; i++) ackpkt.payload[i] = '0';
  ackpkt.checksum = ComputeChecksum(ackpkt);
  tolayer3(B, ackpkt);
}

void B_init(void)
{
  expected_base = 0;
  B_nextseqnum = 1;
  for (int i = 0; i < SEQSPACE; i++) {
    received[i] = false;
  }
}

void B_output(struct msg message) {}
void B_timerinterrupt(void) {}
