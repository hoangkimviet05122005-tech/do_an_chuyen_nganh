/* Ghi 10 s du lieu ECG va gui goi tin qua UART1 (115200) */
#include "main.h"
#include "ecg_config.h"
#include "ecg_state.h"
#include "ecg_uart.h"

extern UART_HandleTypeDef huart1;

typedef struct __attribute__((packed)) {
    uint8_t  sync[2];
    uint16_t fs;
    uint16_t n;
    uint16_t bpm;
    uint16_t flags;
    uint16_t data[REC_N];
    uint8_t  chk;
} EcgPacket;

static EcgPacket pkt;
static uint16_t  recIdx = 0;

void EcgUart_Reset(void)
{
    recIdx = 0;
}

void EcgUart_RecordSample(uint16_t raw)
{
    pkt.data[recIdx++] = raw;

    if (recIdx >= REC_N) {
        const uint8_t *b;
        uint8_t sum = 0;

        pkt.sync[0] = 0xAA;
        pkt.sync[1] = 0x55;
        pkt.fs      = FS_HZ;
        pkt.n       = REC_N;
        pkt.bpm     = g_bpm;
        pkt.flags   = (uint16_t)((g_leadOff ? 1 : 0) | ((g_alarmType ? 1 : 0) << 1));

        b = (const uint8_t *)&pkt + 2;
        for (uint16_t i = 0; i < 8 + REC_N * 2; i++) sum = (uint8_t)(sum + b[i]);
        pkt.chk = sum;

        HAL_UART_Transmit(&huart1, (uint8_t *)&pkt, sizeof(EcgPacket), 3000);
        recIdx = 0;
    }
}
