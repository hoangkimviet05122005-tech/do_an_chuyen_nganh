/* Dieu phoi chinh: noi cac module lai voi nhau
 * AD8232 -> ADC1 (PA0) -> DSP -> Nokia 5110 -> UART1 */
#include "main.h"
#include "ecg_config.h"
#include "ecg_state.h"
#include "ecg_sampling.h"
#include "ecg_dsp.h"
#include "ecg_status.h"
#include "ecg_display.h"
#include "ecg_uart.h"
#include "nokia5110.h"
#include "buttons.h"
#include "ecg_app.h"

static bool     flatSig = true;
static uint16_t winMin = 4095, winMax = 0, winCnt = 0;

static void flatDetectReset(void)
{
    flatSig = true; winMin = 4095; winMax = 0; winCnt = 0;
}

void ECG_Reset(void)
{
    g_muteUntil = HAL_GetTick() + MUTE_MS;
    Dsp_Reset();
    Sampling_Flush();
    EcgUart_Reset();
    flatDetectReset();
    Disp_ScreenInit();
    Disp_DrawHeader(true);
}

void ECG_Init(void)
{
    g_muteUntil = HAL_GetTick() + MUTE_MS;

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET);     /* SDN */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_RESET);   /* buzzer */
    Nokia_Backlight(1);

    Nokia_Init();
    Disp_ScreenInit();
    Disp_DrawHeader(true);

    Sampling_Init();   /* cuoi cung: bat timer lay mau */
}

void ECG_Loop(void)
{
    static uint32_t tHeader = 0;
    uint32_t now = HAL_GetTick();
    uint16_t raw;

    Buttons_Handle(now);
    Status_Update();

    while (Sampling_Pop(&raw)) {
        float norm;
        bool  noSignal;

        if (raw < winMin) winMin = raw;
        if (raw > winMax) winMax = raw;
        if (++winCnt >= FS_HZ) {
            flatSig = ((int)(winMax - winMin) < FLAT_THRESH);
            winMin = 4095; winMax = 0; winCnt = 0;
        }
        noSignal = g_leadOff || flatSig;

        if (g_leadOff) { Dsp_Reset(); norm = 0.0f; }
        else           { norm = Dsp_Process(raw); }

        EcgUart_RecordSample(raw);

        if (noSignal) {
            if (!Disp_NoSignalShown()) Disp_DrawNoSignal();
        } else {
            if (Disp_NoSignalShown()) Disp_ClearWave();
            Disp_PushSample(norm);
        }
    }

    Disp_Refresh();

    if (now - tHeader >= HEADER_MS) {
        tHeader = now;
        Disp_DrawHeader(false);
    }

    Buzzer_Update(now);
}
