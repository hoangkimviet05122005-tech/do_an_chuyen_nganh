#ifndef ECG_STATUS_H
#define ECG_STATUS_H

#include <stdint.h>

void Status_Update(void);              /* doc LO+/LO-, cap nhat g_status, g_alarmType */
void Buzzer_Update(uint32_t now);

#endif
