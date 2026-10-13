#ifndef ECG_STATE_H
#define ECG_STATE_H

#include <stdint.h>
#include <stdbool.h>

typedef enum { MODE_ANALOG = 0, MODE_DIGITAL = 1 } DispMode;
typedef enum { ST_SEARCH, ST_OK, ST_LOW, ST_HIGH, ST_LEADOFF } Status;

/* Trang thai dung chung giua cac module */
extern DispMode g_mode;
extern Status   g_status;
extern uint8_t  g_alarmType;    /* 0: khong, 1: nhip bat thuong, 2: roi dien cuc */
extern bool     g_leadOff;
extern uint16_t g_bpm;
extern uint32_t g_muteUntil;

#endif
