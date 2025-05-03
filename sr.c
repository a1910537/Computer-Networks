#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "emulator.h"
#include "sr.h"

/* ******************************************************************
   Selective Repeat protocol implementation (sr.c). 
   Based on Go-Back-N (gbn.c) but modified for Selective Repeat.
   ******************************************************************/

#define RTT        16.0      /* Round Trip Time; MUST be 16.0 when submitting assignment */
#define WINDOWSIZE 6         /* sender/receiver window size; MUST be 6 when submitting */
#define SEQSPACE   12        /* sequence number space size (at least 2*WINDOWSIZE for SR) */
#define NOTINUSE   (-1)      /* value for unused packet fields (e.g., acknum in data packets) */

/* Compute the checksum of a packet (used by sender and receiver) */
int ComputeChecksum(struct pkt packet) {
    int checksum = 0;
    int i;
    checksum += packet.seqnum;
    checksum += packet.acknum;
    for (i = 0; i < 20; i++) {
        checksum += (int) packet.payload[i];
    }
    return checksum;
}

/* Check if a packet is corrupted by comparing to its checksum */
bool IsCorrupted(struct pkt packet) {
    return (packet.checksum != ComputeChecksum(packet));
}

/********* Sender (A) variables and functions ************/

static int A_base;                     /* Lowest seq number sent but not yet ACKed (window base) */
static int A_nextseqnum;               /* Next sequence number to use for a new packet */
static struct pkt A_buffer[SEQSPACE];  /* Buffer of sent packets awaiting ACK (indexed by seqnum) */
static bool A_ack[SEQSPACE];           /* ACK status for each seq number (true if that seq is ACKed) */

/* Called from layer5: send a new message as a packet if window is not full */
void A_output(struct msg message) {
    struct pkt sendpkt;
    int i;
    /* If window is not full, we can send the new packet */
    if (((A_nextseqnum - A_base + SEQSPACE) % SEQSPACE) < WINDOWSIZE) {
        if (TRACE > 1) {
            printf("----A: New message arrives, send window is not full, send new message to layer3!\n");
        }
        /* Create the packet */
        sendpkt.seqnum = A_nextseqnum;
        sendpkt.acknum = NOTINUSE;  /* not used for data packets */
        for (i = 0; i < 20; i++) {
            sendpkt.payload[i] = message.data[i];
        }
        sendpkt.checksum = ComputeChecksum(sendpkt);
        /* Save packet in the buffer for potential retransmission */
        A_buffer[A_nextseqnum] = sendpkt;
        A_ack[A_nextseqnum] = false;  /* not yet acknowledged */
        /* Send the packet into the network layer */
        if (TRACE > 0) {
            printf("Sending packet %d to layer 3\n", sendpkt.seqnum);
        }
        tolayer3(A, sendpkt);
        /* If this packet is the first in window (base), start the timer */
        if (A_base == A_nextseqnum) {
            starttimer(A, RTT);
        }
        /* Update the next sequence number (wrap around mod SEQSPACE) */
        A_nextseqnum = (A_nextseqnum + 1) % SEQSPACE;
    } else {
        /* Window is full – cannot send new packet, message is dropped */
        if (TRACE > 0) {
            printf("----A: New message arrives, send window is full\n");
        }
        window_full++;
    }
}

/* Called from layer3 at sender A: process an incoming ACK packet */
void A_input(struct pkt packet) {
    if (!IsCorrupted(packet)) {
        if (TRACE > 0) {
            printf("----A: uncorrupted ACK %d is received\n", packet.acknum);
        }
        total_ACKs_received++;
        if (A_base != A_nextseqnum) {  /* Only if there are packets in flight */
            /* Check if ACK is for a seq in the current window */
            int last_seq = (A_nextseqnum + SEQSPACE - 1) % SEQSPACE;  /* seq number of last sent packet (wrap-aware) */
            bool ackInWindow;
            if (A_base <= last_seq) {
                ackInWindow = (packet.acknum >= A_base && packet.acknum <= last_seq);
            } else {
                /* Window wraps around 0 */
                ackInWindow = (packet.acknum >= A_base || packet.acknum <= last_seq);
            }
            if (ackInWindow) {
                if (!A_ack[packet.acknum]) {
                    /* This is a new ACK that we have not seen before */
                    if (TRACE > 0) {
                        printf("----A: ACK %d is not a duplicate\n", packet.acknum);
                    }
                    new_ACKs++;
                    A_ack[packet.acknum] = true;  /* mark that seq as ACKed */
                    if (packet.acknum == A_base) {
                        /* ACK is for the base of the window: slide window forward */
                        stoptimer(A);
                        /* Slide window base forward over this and any subsequent ACKed packets */
                        do {
                            A_ack[A_base] = false;                     /* clear flag as we remove packet from window */
                            A_base = (A_base + 1) % SEQSPACE;
                        } while (A_base != A_nextseqnum && A_ack[A_base]);
                        /* Restart timer if there are still packets awaiting ACK */
                        if (A_base != A_nextseqnum) {
                            starttimer(A, RTT);
                        }
                    }
                    /* If ACK is for a packet not at base, we mark it ACKed above but do not move base yet */
                } else {
                    /* Duplicate ACK (this seqnum was already ACKed before) */
                    if (TRACE > 0) {
                        printf("----A: duplicate ACK received, do nothing!\n");
                    }
                }
            } else {
                /* ACK for a packet that is outside the current window (possibly a delayed ACK for an old packet) */
                if (TRACE > 0) {
                    printf("----A: duplicate ACK received, do nothing!\n");
                }
            }
        } else {
            /* No packets in flight, so this ACK is not relevant (likely duplicate) */
            if (TRACE > 0) {
                printf("----A: duplicate ACK received, do nothing!\n");
            }
        }
    } else {
        /* Corrupted ACK packet */
        if (TRACE > 0) {
            printf("----A: corrupted ACK is received, do nothing!\n");
        }
    }
}

/* Called when A's timer expires (timeout for the oldest unACKed packet) */
void A_timerinterrupt(void) {
    /* Time out event: resend the packet at A_base */
    if (TRACE > 0) {
        printf("----A: time out, resend packet %d\n", A_base);
    }
    tolayer3(A, A_buffer[A_base]);   /* retransmit the oldest unACKed packet */
    packets_resent++;
    starttimer(A, RTT);             /* restart timer for next timeout */
}

/* Initialization function for A, called once before any others */
void A_init(void) {
    int i;
    A_base = 0;
    A_nextseqnum = 0;
    for (i = 0; i < SEQSPACE; i++) {
        A_ack[i] = false;
    }
}

/********* Receiver (B) variables and functions ************/

static int B_expectedseq;            /* Next sequence number expected (base of B's receive window) */
static bool B_received[SEQSPACE];    /* Marks if a packet with given seqnum has been received (buffered but not delivered) */
static char B_buffer[SEQSPACE][20];  /* Buffers payloads of out-of-order packets */
static int B_nextackseq;             /* Next sequence number to use in ACK packets (for logging purposes) */

/* Called from layer3 at receiver B: process an incoming data packet */
void B_input(struct pkt packet) {
    struct pkt ackpkt;
    int i;
    if (!IsCorrupted(packet)) {
        /* Packet is not corrupted */
        int seq_high = (B_expectedseq + WINDOWSIZE - 1) % SEQSPACE;  /* seqnum at far end of receive window */
        bool inWindow;
        if (B_expectedseq <= seq_high) {
            /* No wrap-around in window */
            inWindow = (packet.seqnum >= B_expectedseq && packet.seqnum <= seq_high);
        } else {
            /* Window wraps around sequence space */
            inWindow = (packet.seqnum >= B_expectedseq || packet.seqnum <= seq_high);
        }
        if (inWindow) {
            if (!B_received[packet.seqnum]) {
                /* New packet within the window (not a duplicate) */
                B_received[packet.seqnum] = true;
                for (i = 0; i < 20; i++) {
                    B_buffer[packet.seqnum][i] = packet.payload[i];
                }
                packets_received++;
            }
            if (packet.seqnum == B_expectedseq) {
                /* This packet is the next expected in order */
                if (TRACE > 0) {
                    printf("----B: packet %d is correctly received, send ACK!\n", packet.seqnum);
                }
                /* Deliver packet to application layer */
                tolayer5(B, packet.payload);
                B_received[packet.seqnum] = false;
                B_expectedseq = (B_expectedseq + 1) % SEQSPACE;
                /* Deliver any subsequently buffered packets in order */
                while (B_received[B_expectedseq]) {
                    /* Packet B_expectedseq has arrived earlier and is buffered */
                    struct pkt deliverpkt;
                    for (i = 0; i < 20; i++) {
                        deliverpkt.payload[i] = B_buffer[B_expectedseq][i];
                    }
                    deliverpkt.seqnum = B_expectedseq;  /* (not used by tolayer5, just for clarity) */
                    tolayer5(B, deliverpkt.payload);
                    B_received[B_expectedseq] = false;
                    B_expectedseq = (B_expectedseq + 1) % SEQSPACE;
                }
            } else {
                /* Packet is out-of-order (within window) */
                if (TRACE > 0) {
                    printf("----B: packet %d is out-of-order, buffer and ACK!\n", packet.seqnum);
                }
                /* (Out-of-order packet is buffered above, not delivered yet) */
            }
            /* Prepare an ACK for this packet */
            ackpkt.acknum = packet.seqnum;
        } else {
            /* Packet is outside the current receive window */
            if (packet.seqnum < B_expectedseq) {
                /* An already delivered packet (duplicate) */
                if (TRACE > 0) {
                    printf("----B: packet corrupted or not expected sequence number, resend ACK!\n");
                }
                /* Re-ACK this packet (sender might not have received the original ACK) */
                ackpkt.acknum = packet.seqnum;
            } else {
                /* Packet is beyond the receiver's window (not yet expecting it) */
                if (TRACE > 0) {
                    printf("----B: packet corrupted or not expected sequence number, resend ACK!\n");
                }
                /* Send ACK for the last in-order packet (one just before B_expectedseq) */
                ackpkt.acknum = (B_expectedseq + SEQSPACE - 1) % SEQSPACE;
            }
        }
    } else {
        /* Packet was corrupted */
        if (TRACE > 0) {
            printf("----B: packet corrupted or not expected sequence number, resend ACK!\n");
        }
        /* ACK the last correctly received in-order packet */
        if (B_expectedseq == 0) {
            ackpkt.acknum = SEQSPACE - 1;
        } else {
            ackpkt.acknum = (B_expectedseq - 1) % SEQSPACE;
        }
    }
    /* Create the ACK packet to send back to A */
    ackpkt.seqnum = B_nextackseq;
    B_nextackseq = (B_nextackseq + 1) % 2;   /* sequence field for ACK packets (not really used by A) */
    for (i = 0; i < 20; i++) {
        ackpkt.payload[i] = '0';            /* payload not used in ACK, fill with dummy data */
    }
    ackpkt.checksum = ComputeChecksum(ackpkt);
    tolayer3(B, ackpkt);  /* Send ACK packet to A */
}

/* Initialization function for B, called once before any other B-side functions */
void B_init(void) {
    int i;
    B_expectedseq = 0;
    B_nextackseq = 1;
    for (i = 0; i < SEQSPACE; i++) {
        B_received[i] = false;
    }
}

/* Note: B_output and B_timerinterrupt are not used in this one-direction protocol */
void B_output(struct msg message) {
    /* Not needed (no data from B to A in this assignment) */
}

void B_timerinterrupt(void) {
    /* Not used at receiver */
}
