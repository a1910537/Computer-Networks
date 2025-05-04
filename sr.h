#ifndef SR_H
#define SR_H

#include "emulator.h"

/* 初始化发送方 */
extern void A_init(void);

/* 初始化接收方 */
extern void B_init(void);

/* 发送方从应用层收到消息 */
extern void A_output(struct msg);

/* 发送方从网络层收到 ACK */
extern void A_input(struct pkt);

/* 发送方计时器中断 */
extern void A_timerinterrupt(void);

/* 接收方从网络层收到数据包 */
extern void B_input(struct pkt);

/* 以下函数用于双向通信扩展，本项目未使用 */
extern void B_output(struct msg);
extern void B_timerinterrupt(void);

#endif /* SR_H */
