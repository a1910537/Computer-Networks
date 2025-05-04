#ifndef SR_H
#define SR_H

#include "emulator.h"

/* === 发送方 A 的函数 === */
void A_init(void);                  // 初始化发送方
void A_output(struct msg message);  // 应用层给发送方消息
void A_input(struct pkt packet);    // 收到 ACK
void A_timerinterrupt(void);        // 计时器中断（触发重传）

/* === 接收方 B 的函数 === */
void B_init(void);                  // 初始化接收方
void B_input(struct pkt packet);    // 收到数据包
void B_output(struct msg message);  // 不使用，但必须定义（用于双向）
void B_timerinterrupt(void);        // 不使用，但必须定义（用于双向）

/* === 辅助函数 === */
int ComputeChecksum(struct pkt packet);  // 计算校验和
bool IsCorrupted(struct pkt packet);     // 判断是否损坏

#endif /* SR_H */
