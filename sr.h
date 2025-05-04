#ifndef SR_H
#define SR_H

void A_init(void);
void A_output(struct msg message);
void A_input(struct pkt packet);
void A_timerinterrupt(void);

void B_init(void);
void B_input(struct pkt packet);
void B_output(struct msg message);
void B_timerinterrupt(void);

int ComputeChecksum(struct pkt packet);
int IsCorrupted(struct pkt packet);

#endif
