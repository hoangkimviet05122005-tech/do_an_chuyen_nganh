#ifndef ECG_CONFIG_H
#define ECG_CONFIG_H

/* ===== Cau hinh chung cho he thong do ECG ===== */
#define FS_HZ          250                 /* tan so lay mau */
#define REC_SECONDS    10
#define REC_N          (FS_HZ * REC_SECONDS)

#define BPM_LOW        50
#define BPM_HIGH       120

#define FIFO_SIZE      512
#define FLAT_THRESH    30                  /* bien do toi thieu de coi la co tin hieu */

#define DEBOUNCE_MS    30
#define HEADER_MS      200
#define MUTE_MS        10000

#endif
