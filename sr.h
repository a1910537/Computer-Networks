#ifndef SR_H
#define SR_H

void A_init(void);
void B_init(void);
void A_input(struct pkt);
void B_input(struct pkt);
void A_output(struct msg);
void A_timerinterrupt(void);

#define BIDIRECTIONAL 0
void B_output(struct msg);
void B_timerinterrupt(void);

#endif
