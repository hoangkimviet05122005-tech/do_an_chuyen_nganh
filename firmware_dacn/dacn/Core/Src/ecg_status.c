/* Trang thai he thong + coi canh bao
 *   PB0 LO+ | PB1 LO- | PB11 buzzer */
#include "main.h"
#include "ecg_config.h"
#include "ecg_state.h"
#include "ecg_status.h"

void Status_Update(void)
{
    static uint32_t tLoChange = 0;
    static bool     loRaw = false, loStable = false;
    uint32_t now = HAL_GetTick();

    bool r = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_SET) ||
             (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1) == GPIO_PIN_SET);

    if (r != loRaw) { loRaw = r; tLoChange = now; }
    if ((now - tLoChange) >= 200) loStable = loRaw;   /* chi doi khi on dinh 200ms */
    g_leadOff = loStable;

    if (g_leadOff)                  { g_status = ST_LEADOFF; g_alarmType = 2; }
    else if (g_bpm == 0)            { g_status = ST_SEARCH;  g_alarmType = 0; }
    else if (g_bpm < BPM_LOW)       { g_status = ST_LOW;     g_alarmType = 1; }
    else if (g_bpm > BPM_HIGH)      { g_status = ST_HIGH;    g_alarmType = 1; }
    else                            { g_status = ST_OK;      g_alarmType = 0; }
}

void Buzzer_Update(uint32_t now)
{
    static uint32_t t = 0;
    static uint8_t  on = 0;
    uint32_t period;

    if ((int32_t)(now - g_muteUntil) < 0 || g_alarmType == 0) {
        on = 0;
        t = now;
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_RESET);
        return;
    }
    period = (g_alarmType == 1) ? 150 : 500;
    if (now - t >= period) {
        t = now;
        on ^= 1;
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}
