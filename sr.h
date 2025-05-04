#ifndef SR_H
#define SR_H

#include "emulator.h"

/* 用 typedef 兼容 C90 的 bool 类型 */
typedef enum { false = 0, true = 1 } bool;

/* === A 端函数 === */
void A_init(void);
void A_output(struct msg message);
void A_input(struct pkt packet);
void A_timerinterrupt(void);

/* === B 端函数 === */
void B_init(void);
void B_input(struct pkt packet);
void B_output(struct msg message);
void B_timerinterrupt(void);

/* === 工具函数 === */
int ComputeChecksum(struct pkt packet);
bool IsCorrupted(struct pkt packet);

#endif /* SR_H */
