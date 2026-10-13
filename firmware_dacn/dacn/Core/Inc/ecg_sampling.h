#ifndef ECG_SAMPLING_H
#define ECG_SAMPLING_H

#include <stdint.h>
#include <stdbool.h>

void Sampling_Init(void);          /* cau hinh ADC + FIFO + bat TIM3 250 Hz */
bool Sampling_Pop(uint16_t *v);    /* lay 1 mau tu FIFO, false neu rong */
void Sampling_Flush(void);         /* xoa het mau trong FIFO */

#endif
