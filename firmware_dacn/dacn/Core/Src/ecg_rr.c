#include "ecg_rr.h"

#define MS_TO_SAMPLES(ms)   (((ms) * ECG_FS_HZ) / 1000u)
#define SAMPLES_TO_MS(n)    (((n) * 1000u) / ECG_FS_HZ)

static int32_t  baseline_q8;     /* duong nen (Q8) */
static int32_t  env;             /* bao dinh (envelope) */
static int32_t  x1, x2;          /* mau truoc, mau truoc nua (da tru nen) */
static uint32_t tick;            /* dem mau */
static uint32_t last_r_tick;
static uint8_t  has_last_r;

static uint16_t rr_buf[ECG_RR_BUF_LEN];
static uint8_t  rr_count;        /* so khoang hop le dang luu (<= BUF_LEN) */
static uint8_t  rr_head;

void ecg_rr_reset(void)
{
    env = ECG_RR_MIN_PEAK;
    x1 = x2 = 0;
    has_last_r = 0;
    rr_count = 0;
    rr_head = 0;
}

void ecg_rr_init(void)
{
    baseline_q8 = 0;
    tick = 0;
    last_r_tick = 0;
    ecg_rr_reset();
}

static void rr_push(uint16_t ms)
{
    rr_buf[rr_head] = ms;
    rr_head = (uint8_t)((rr_head + 1u) % ECG_RR_BUF_LEN);
    if (rr_count < ECG_RR_BUF_LEN) rr_count++;
}

uint8_t ecg_rr_feed(int16_t sample)
{
    uint8_t found = 0;
    int32_t x, thr;

    /* 1) Tru duong nen bang IIR (hang so thoi gian ~64 mau) */
    if (tick == 0) baseline_q8 = ((int32_t)sample) << 8;
    baseline_q8 += ((((int32_t)sample) << 8) - baseline_q8) >> 6;
    x = (int32_t)sample - (baseline_q8 >> 8);

    /* 2) Bao dinh: len nhanh, xuong cham (~1 s) */
    if (x > env) env = x;
    else         env -= (env >> 8);
    if (env < ECG_RR_MIN_PEAK) env = ECG_RR_MIN_PEAK;

    thr = (env * 5) / 8;   /* nguong = 62.5% dinh */

    /* 3) Dinh cuc bo tai mau x1: x1 > x2, x1 >= x, x1 > thr */
    if (tick >= 2u && x1 > x2 && x1 >= x && x1 > thr) {
        uint32_t peak_tick = tick - 1u;

        if (!has_last_r) {
            last_r_tick = peak_tick;
            has_last_r = 1;
            found = 1;
        } else {
            uint32_t dt = peak_tick - last_r_tick;
            if (dt >= MS_TO_SAMPLES(ECG_RR_REFRACT_MS)) {
                uint32_t ms = SAMPLES_TO_MS(dt);
                last_r_tick = peak_tick;
                found = 1;
                if (ms >= ECG_RR_MIN_MS && ms <= ECG_RR_MAX_MS) {
                    rr_push((uint16_t)ms);
                } else {
                    rr_count = 0;   /* khoang bat thuong -> bat dau lai chuoi */
                }
            }
        }
    }

    /* 4) Qua lau khong thay R -> mat tin hieu */
    if (has_last_r && (tick - last_r_tick) > MS_TO_SAMPLES(ECG_RR_TIMEOUT_MS)) {
        has_last_r = 0;
        rr_count = 0;
    }

    x2 = x1;
    x1 = x;
    tick++;
    return found;
}

uint16_t ecg_rr_get_last_ms(void)
{
    if (rr_count == 0) return 0;
    return rr_buf[(rr_head + ECG_RR_BUF_LEN - 1u) % ECG_RR_BUF_LEN];
}

uint16_t ecg_rr_get_avg_ms(void)
{
    uint32_t sum = 0;
    uint8_t i;
    if (rr_count == 0) return 0;
    for (i = 0; i < rr_count; i++) sum += rr_buf[i];
    return (uint16_t)(sum / rr_count);
}

uint16_t ecg_rr_get_bpm(void)
{
    uint16_t avg = ecg_rr_get_avg_ms();
    if (avg == 0) return 0;
    return (uint16_t)(60000u / avg);
}

uint8_t ecg_rr_is_irregular(void)
{
    uint32_t avg;
    uint32_t limit;
    uint8_t i;

    if (rr_count < ECG_RR_MIN_FOR_IRREG) return 0;
    avg = ecg_rr_get_avg_ms();
    limit = (avg * ECG_RR_IRREG_PCT) / 100u;

    for (i = 0; i < rr_count; i++) {
        uint32_t d = (rr_buf[i] > avg) ? (rr_buf[i] - avg) : (avg - rr_buf[i]);
        if (d > limit) return 1;
    }
    return 0;
}