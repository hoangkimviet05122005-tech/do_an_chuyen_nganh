/* Xu ly tin hieu: loc, tim dinh R, tinh BPM */
#include <math.h>
#include "ecg_config.h"
#include "ecg_state.h"
#include "ecg_dsp.h"

static float    lp, base, env;
static bool     dspInit = false;
static uint32_t sIdx = 0, lastPeak = 0;
static bool     above = false;
static uint16_t rr[4];
static uint8_t  rrN = 0, rrPos = 0;

void Dsp_Reset(void)
{
    dspInit = false;
    sIdx = 0; lastPeak = 0; above = false;
    rrN = 0; rrPos = 0; g_bpm = 0;
}

float Dsp_Process(uint16_t raw)
{
    float d, a, thr;

    if (!dspInit) { lp = base = (float)raw; env = 100.0f; dspInit = true; }

    lp   += 0.4f  * ((float)raw - lp);
    base += 0.01f * (lp - base);
    d = lp - base;
    a = fabsf(d);

    env = (a > env) ? a : env * 0.999f;
    if (env < 60.0f) env = 60.0f;

    sIdx++;
    thr = 0.55f * env;

    if (a > thr) {
        if (!above && (sIdx - lastPeak) > (uint32_t)(FS_HZ * 3 / 10)) {
            if (lastPeak != 0) {
                uint32_t r = sIdx - lastPeak;
                if (r < (uint32_t)(FS_HZ * 2)) {
                    rr[rrPos] = (uint16_t)r;
                    rrPos = (uint8_t)((rrPos + 1) % 4);
                    if (rrN < 4) rrN++;
                    if (rrN >= 2) {
                        uint32_t sum = 0, avg;
                        for (uint8_t i = 0; i < rrN; i++) sum += rr[i];
                        avg = sum / rrN;
                        g_bpm = (uint16_t)((60UL * FS_HZ) / avg);
                    }
                } else {
                    rrN = 0; rrPos = 0;
                }
            }
            lastPeak = sIdx;
        }
        above = true;
    } else if (a < thr * 0.5f) {
        above = false;
    }

    if (g_bpm && (sIdx - lastPeak) > (uint32_t)(FS_HZ * 3)) {
        g_bpm = 0; rrN = 0; rrPos = 0;
    }

    return d / (env * 1.2f);
}
