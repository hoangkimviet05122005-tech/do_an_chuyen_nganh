/* Hien thi tren Nokia 5110: header (BPM, che do, trang thai) + song ECG */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "ecg_state.h"
#include "nokia5110.h"
#include "ecg_display.h"

#define WAVE_Y0        10
#define WAVE_Y1        (LCD_H - 1)
#define WAVE_YC        ((WAVE_Y0 + WAVE_Y1) / 2)
#define WAVE_HALF      (((WAVE_Y1 - WAVE_Y0) / 2) - 2)
#define DISP_DECIM     1
#define CLEAR_AHEAD    2
#define REFRESH_MS     30   /* ~33 khung hinh/giay de tranh nhap nhay */

static int     colX = 0, prevY = WAVE_YC;
static float   acc = 0.0f;
static uint8_t accN = 0;
static bool    noSigShown = false;

void Disp_DrawHeader(bool force)
{
    static char o1[10], o2[10], o3[10];
    char s1[10], s2[10], s3[10];

    if (g_bpm) snprintf(s1, sizeof s1, "B:%3u", (unsigned)g_bpm);
    else       snprintf(s1, sizeof s1, "B:---");

    snprintf(s3, sizeof s3, "%s", (g_mode == MODE_ANALOG) ? "ANA" : "DIG");

    switch (g_status) {
        case ST_OK:      snprintf(s2, sizeof s2, "OK "); break;
        case ST_LOW:     snprintf(s2, sizeof s2, "LOW"); break;
        case ST_HIGH:    snprintf(s2, sizeof s2, "HI "); break;
        case ST_LEADOFF: snprintf(s2, sizeof s2, "OFF"); break;
        default:         snprintf(s2, sizeof s2, "SRI"); break;
    }

    if (force || strcmp(s1, o1)) { Nokia_DrawString(0,  0, s1); strcpy(o1, s1); }
    if (force || strcmp(s3, o3)) { Nokia_DrawString(33, 0, s3); strcpy(o3, s3); }
    if (force || strcmp(s2, o2)) { Nokia_DrawString(54, 0, s2); strcpy(o2, s2); }
}

void Disp_ResetAccumulator(void)
{
    acc = 0.0f; accN = 0;
}

void Disp_ClearWave(void)
{
    Nokia_ClearArea(0, WAVE_Y0, LCD_W - 1, WAVE_Y1);
    colX = 0; prevY = WAVE_YC;
    Disp_ResetAccumulator();
    noSigShown = false;
}

void Disp_ScreenInit(void)
{
    Nokia_Clear();
    for (int x = 0; x < LCD_W; x++) Nokia_DrawPixel(x, WAVE_Y0 - 1, 1);  /* gach phan cach */
    Disp_ClearWave();
}

void Disp_DrawNoSignal(void)
{
    Disp_ClearWave();
    Nokia_DrawString(2, 3, "NO SIGNAL");   /* hang 3 (y ~ 24 pixel) */
    noSigShown = true;
}

bool Disp_NoSignalShown(void)
{
    return noSigShown;
}

static void plotValue(float norm)
{
    int y, xe;

    if (g_mode == MODE_ANALOG) {
        if (norm >  1.0f) norm =  1.0f;
        if (norm < -1.0f) norm = -1.0f;
        y = WAVE_YC - (int)(norm * WAVE_HALF);
    } else {
        /* Digital: song vuong 1/0 co hysteresis */
        static uint8_t level = 0;
        float a = fabsf(norm);

        if (a > 0.45f)      level = 1;
        else if (a < 0.25f) level = 0;

        y = level ? (WAVE_YC - WAVE_HALF) : (WAVE_YC + WAVE_HALF);
    }

    xe = colX + CLEAR_AHEAD;
    if (xe > LCD_W - 1) xe = LCD_W - 1;
    Nokia_ClearArea(colX, WAVE_Y0, xe, WAVE_Y1);

    Nokia_DrawVLine(colX, prevY, y, 1);

    prevY = y;
    if (++colX >= LCD_W) colX = 0;
}

void Disp_PushSample(float norm)
{
    acc += norm;
    if (++accN >= DISP_DECIM) {
        plotValue(acc / accN);
        acc = 0.0f; accN = 0;
    }
}

void Disp_Refresh(void)
{
    static uint32_t tRefresh = 0;
    uint32_t now = HAL_GetTick();

    if (now - tRefresh >= REFRESH_MS) {
        tRefresh = now;
        Nokia_Refresh();
    }
}
