#ifndef ECG_DISPLAY_H
#define ECG_DISPLAY_H

#include <stdbool.h>

void Disp_ScreenInit(void);
void Disp_DrawHeader(bool force);
void Disp_ClearWave(void);
void Disp_DrawNoSignal(void);
bool Disp_NoSignalShown(void);
void Disp_PushSample(float norm);   /* gom DISP_DECIM mau roi ve 1 cot */
void Disp_Refresh(void);            /* day buffer ra Nokia, tu gioi han 30 ms */
void Disp_ResetAccumulator(void);

#endif
