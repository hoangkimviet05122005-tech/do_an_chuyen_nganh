#include "main.h"
#include "ecg_config.h"
#include "ecg_state.h"
#include "ecg_display.h"
#include "ecg_app.h"
#include "buttons.h"

static bool btnEdge(GPIO_TypeDef *port, uint16_t pin, uint8_t *state, uint32_t *t, uint32_t now)
{
    uint8_t r = (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET);
    if (r != *state && (now - *t) >= DEBOUNCE_MS) {
        *t = now;
        *state = r;
        return r;
    }
    return false;
}

void Buttons_Handle(uint32_t now)
{
    static uint8_t  sUp = 0, sDn = 0;
    static uint32_t tUp = 0, tDn = 0;

    if (btnEdge(GPIOA, GPIO_PIN_6, &sUp, &tUp, now)) {
        g_mode = (g_mode == MODE_ANALOG) ? MODE_DIGITAL : MODE_ANALOG;
        Disp_ClearWave();
        Disp_DrawHeader(true);
    }
    if (btnEdge(GPIOA, GPIO_PIN_8, &sDn, &tDn, now)) {
        ECG_Reset();
    }
}
