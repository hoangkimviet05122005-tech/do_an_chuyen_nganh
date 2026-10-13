#ifndef ECG_UART_H
#define ECG_UART_H

#include <stdint.h>

void EcgUart_Reset(void);
void EcgUart_RecordSample(uint16_t raw);   /* du REC_N mau thi gui 1 goi qua UART1 */

#endif
