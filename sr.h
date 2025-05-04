#ifndef SR_H
#define SR_H

struct msg {
  char data[20];
};

struct pkt {
  int seqnum;
  int acknum;
  int checksum;
  char payload[20];
};

void A_init(void);
void A_output(struct msg message);
void A_input(struct pkt packet);
void A_timerinterrupt(void);

void B_init(void);
void B_input(struct pkt packet);
void B_output(struct msg message);
void B_timerinterrupt(void);

#endif
