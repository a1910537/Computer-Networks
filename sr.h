#ifndef SR_H
#define SR_H

#include "emulator.h"

#define WINDOW_SIZE 8
#define SEQSPACE (2 * WINDOW_SIZE)

void A_init();
void A_input(struct pkt packet);
void A_output(struct msg message);
void A_timerinterrupt();

void B_init();
void B_input(struct pkt packet);

#endif
