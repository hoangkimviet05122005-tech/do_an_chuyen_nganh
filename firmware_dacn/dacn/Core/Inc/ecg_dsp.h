#ifndef ECG_DSP_H
#define ECG_DSP_H

#include <stdint.h>

void  Dsp_Reset(void);
float Dsp_Process(uint16_t raw);   /* tra ve gia tri chuan hoa ~[-1,1], cap nhat g_bpm */

#endif
