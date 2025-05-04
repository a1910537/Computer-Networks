#ifndef SR_H
#define SR_H

#include "emulator.h"

#define RTT  16.0       /* round trip time. MUST BE SET TO 16.0 when submitting assignment */
#define WINDOWSIZE 6    /* maximum number of buffered unacknowledged packets */
#define SEQSPACE 12     /* sequence space should be at least 2 * WINDOWSIZE */
#define NOTINUSE (-1)   /* value used for unused header fields */

/* Sender (A) function declarations */
void A_output(struct msg message);
void A_input(struct pkt packet);
void A_timerinterrupt(void);
void A_init(void);

/* Receiver (B) function declarations */
void B_input(struct pkt packet);
void B_init(void);

/* Optional (unused for simplex) */
void B_output(struct msg message);
void B_timerinterrupt(void);

/* Utility functions */
int ComputeChecksum(struct pkt packet);
bool IsCorrupted(struct pkt packet);

/* External statistics variables (defined in emulator.c) */
extern int TRACE;
extern int packets_resent;
extern int packets_received;
extern int total_ACKs_received;
extern int new_ACKs;
extern int window_full;

#endif // SR_H
