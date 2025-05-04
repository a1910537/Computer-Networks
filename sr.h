#ifndef SR_H
#define SR_H

/* 前置声明，避免重复定义结构体 */
struct msg;
struct pkt;

/* A 端接口 */
void A_init(void);
void A_output(struct msg message);
void A_input(struct pkt packet);
void A_timerinterrupt(void);

/* B 端接口 */
void B_init(void);
void B_input(struct pkt packet);
void B_output(struct msg message);
void B_timerinterrupt(void);

/* 工具函数 */
int ComputeChecksum(struct pkt packet);
int IsCorrupted(struct pkt packet);  // 注意：如果你在 .c 里用的是 bool，请改成 int 返回 0/1

#endif /* SR_H */
