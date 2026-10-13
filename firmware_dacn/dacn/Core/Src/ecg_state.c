#include "ecg_state.h"

DispMode g_mode       = MODE_ANALOG;
Status   g_status     = ST_SEARCH;
uint8_t  g_alarmType  = 0;
bool     g_leadOff    = false;
uint16_t g_bpm        = 0;
uint32_t g_muteUntil  = 0;
