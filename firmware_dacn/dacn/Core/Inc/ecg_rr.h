#ifndef ECG_RR_H
#define ECG_RR_H

#include <stdint.h>

#define ECG_FS_HZ            250u
#define ECG_RR_BUF_LEN       8u      /* so khoang R-R luu de tinh trung binh */
#define ECG_RR_MIN_MS        300u    /* > 200 bpm -> bo */
#define ECG_RR_MAX_MS        2000u   /* < 30 bpm  -> bo */
#define ECG_RR_REFRACT_MS    200u    /* thoi gian khong cho phep R moi */
#define ECG_RR_MIN_PEAK      150     /* nguong bien do toi thieu (ADC count), chinh theo tin hieu */
#define ECG_RR_IRREG_PCT     20u     /* lech > 20% so voi trung binh -> bat thuong */
#define ECG_RR_MIN_FOR_IRREG 5u      /* can it nhat 5 khoang R-R moi ket luan */
#define ECG_RR_TIMEOUT_MS    3000u   /* khong thay R trong 3 s -> bpm = 0 */

void     ecg_rr_init(void);
void     ecg_rr_reset(void);                 /* goi khi LEAD OFF */
uint8_t  ecg_rr_feed(int16_t sample);        /* tra 1 neu vua phat hien dinh R */
uint16_t ecg_rr_get_last_ms(void);           /* khoang R-R gan nhat (ms) */
uint16_t ecg_rr_get_avg_ms(void);            /* R-R trung binh (ms) */
uint16_t ecg_rr_get_bpm(void);               /* 0 neu chua co / mat tin hieu */
uint8_t  ecg_rr_is_irregular(void);          /* 1 neu nhip khong deu */

#endif