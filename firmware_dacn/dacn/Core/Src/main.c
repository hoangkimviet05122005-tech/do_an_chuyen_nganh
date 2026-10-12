/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Do ECG AD8232, hien thi Nokia 5110, chuyen doi Che do & truyen UART
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_SPI1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* Do dien tim: AD8232 -> ADC1 (PA0) -> LCD Nokia 5110 -> UART1 -> may tinh
 *   PA0 ADC ECG | PA6 nut UP (doi Analog/Digital) | PA8 nut DOWN (reset)
 *   PB0 LO+ | PB1 LO- | PB10 SDN | PB11 buzzer | PA1 BL (Den nen Nokia 5110)
 *   Man hinh Nokia 5110: PA2 SCE(CS), PA3 DC, PA4 RST, PA5 SCK, PA7 MOSI (SPI1)
 *   Lay mau 250 Hz bang TIM3 (cau hinh bang thanh ghi, khong can CubeMX)
 */

/* ===================== CAU HINH ===================== */
#define FS_HZ          250
#define REC_SECONDS    10
#define REC_N          (FS_HZ * REC_SECONDS)

#define BPM_LOW        50
#define BPM_HIGH       120

/* Man hinh Nokia 5110: 84x48 pixel */
#define LCD_W          84
#define LCD_H          48

#define WAVE_Y0        10
#define WAVE_Y1        (LCD_H - 1)
#define WAVE_YC        ((WAVE_Y0 + WAVE_Y1) / 2)
#define WAVE_HALF      (((WAVE_Y1 - WAVE_Y0) / 2) - 2)
#define DISP_DECIM     1
#define CLEAR_AHEAD    2

#define DEBOUNCE_MS    30
#define HEADER_MS      200
#define MUTE_MS        10000

/* ===================== DRIVER NOKIA 5110 (PCD8544) ===================== */
#define LCD_CS(x)   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define LCD_DC(x)   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define LCD_RST(x)  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define LCD_BL(x)   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)

static uint8_t lcd_buffer[84 * 6]; // 84 cot x 6 hang (moi hang 8 bit = 48 pixel)
static uint16_t lastRaw = 0;
static uint32_t muteUntil = 0;

static void lcdWrite(const uint8_t *d, uint16_t n)
{
    HAL_SPI_Transmit(&hspi1, (uint8_t *)d, n, 100);
}

static void nokiaWriteCmd(uint8_t cmd)
{
    LCD_DC(0); LCD_CS(0);
    lcdWrite(&cmd, 1);
    LCD_CS(1);
}

static void nokiaWriteData(uint8_t data)
{
    LCD_DC(1); LCD_CS(0);
    lcdWrite(&data, 1);
    LCD_CS(1);
}

static void nokiaRefresh(void)
{
    nokiaWriteCmd(0x80); // X = 0
    nokiaWriteCmd(0x40); // Y = 0
    LCD_DC(1); LCD_CS(0);
    lcdWrite(lcd_buffer, sizeof(lcd_buffer));
    LCD_CS(1);
}

static void lcdClear(void)
{
    memset(lcd_buffer, 0, sizeof(lcd_buffer));
}

static void lcdInit(void)
{
    LCD_CS(1);
    LCD_RST(0); HAL_Delay(20);
    LCD_RST(1); HAL_Delay(50);

    nokiaWriteCmd(0x21); // Extended instruction set
    nokiaWriteCmd(0xC8); // Set Vop (Contrast: thu tang giam 0xC0-0xCF neu man hinh mo)
    nokiaWriteCmd(0x06); // Temperature coefficient
    nokiaWriteCmd(0x13); // Bias system (1:48)
    nokiaWriteCmd(0x20); // Standard instruction set, horizontal addressing
    nokiaWriteCmd(0x0C); // Normal display mode (0x0D choche dao nguoc)
}

/* Font 5x7 tieu chu?n */
static const uint8_t font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, // ' '
    {0x08,0x08,0x08,0x08,0x08}, // '-'
    {0x00,0x36,0x36,0x00,0x00}, // ':'
    {0x3E,0x51,0x49,0x45,0x3E}, // '0'
    {0x00,0x42,0x7F,0x40,0x00}, // '1'
    {0x42,0x61,0x51,0x49,0x46}, // '2'
    {0x21,0x41,0x45,0x4B,0x31}, // '3'
    {0x18,0x14,0x12,0x7F,0x10}, // '4'
    {0x27,0x45,0x45,0x45,0x39}, // '5'
    {0x3C,0x4A,0x49,0x49,0x30}, // '6'
    {0x01,0x71,0x09,0x05,0x03}, // '7'
    {0x36,0x49,0x49,0x49,0x36}, // '8'
    {0x06,0x49,0x49,0x29,0x1E}, // '9'
    {0x7E,0x11,0x11,0x11,0x7E}, // 'A'
    {0x7F,0x49,0x49,0x49,0x36}, // 'B'
    {0x3E,0x41,0x41,0x41,0x22}, // 'C'
    {0x7F,0x41,0x41,0x22,0x1C}, // 'D'
    {0x7F,0x49,0x49,0x49,0x41}, // 'E'
    {0x7F,0x09,0x09,0x09,0x01}, // 'F'
    {0x3E,0x41,0x49,0x49,0x7A}, // 'G'
    {0x7F,0x08,0x08,0x08,0x7F}, // 'H'
    {0x00,0x41,0x7F,0x41,0x00}, // 'I'
    {0x7F,0x08,0x14,0x22,0x41}, // 'K'
    {0x7F,0x40,0x40,0x40,0x40}, // 'L'
    {0x7F,0x02,0x0C,0x02,0x7F}, // 'M'
    {0x7F,0x04,0x08,0x10,0x7F}, // 'N'
    {0x3E,0x41,0x41,0x41,0x3E}, // 'O'
    {0x7F,0x09,0x09,0x09,0x06}, // 'P'
    {0x7F,0x09,0x19,0x29,0x46}, // 'R'
    {0x46,0x49,0x49,0x49,0x31}, // 'S'
    {0x01,0x01,0x7F,0x01,0x01}, // 'T'
    {0x3F,0x40,0x40,0x40,0x3F}, // 'U'
    {0x3F,0x40,0x38,0x40,0x3F}, // 'W'
};

static const uint8_t *glyphOf(char ch)
{
    static const uint8_t blank[5] = {0, 0, 0, 0, 0};
    /* Thu tu phai khop DUNG voi bang font5x7 tu 'A' (index 13) tro di */
    static const char map[] = "ABCDEFGHIKLMNOPRSTUW";

    if (ch >= '0' && ch <= '9') return font5x7[3 + (ch - '0')];
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');   /* chu thuong -> hoa */
    if (ch >= 'A' && ch <= 'Z') {
        for (uint8_t j = 0; j < sizeof(map) - 1; j++) {
            if (map[j] == ch) return font5x7[13 + j];
        }
        return blank;
    }
    if (ch == ' ') return font5x7[0];
    if (ch == '-') return font5x7[1];
    if (ch == ':') return font5x7[2];
    return blank;
}

static void nokiaDrawChar(uint8_t x, uint8_t y, char ch)
{
    const uint8_t *g = glyphOf(ch);
    for (uint8_t i = 0; i < 5; i++) {
        if ((x + i) < LCD_W && y < 6) {
            lcd_buffer[x + i + (y * 84)] = g[i];
        }
    }
}

static void nokiaDrawString(uint8_t x, uint8_t y, const char *s)
{
    while (*s) {
        nokiaDrawChar(x, y, *s++);
        x += 6; // Moi ky tu cach nhau 6 pixel
        if (x > LCD_W - 6) break;
    }
}

static void nokiaDrawPixel(int x, int y, uint8_t color)
{
    if (x < 0 || x >= LCD_W || y < 0 || y >= LCD_H) return;
    if (color) {
        lcd_buffer[x + ((y / 8) * 84)] |= (1 << (y % 8));
    } else {
        lcd_buffer[x + ((y / 8) * 84)] &= ~(1 << (y % 8));
    }
}

static void nokiaDrawVLine(int x, int y0, int y1, uint8_t color)
{
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    for (int y = y0; y <= y1; y++) {
        nokiaDrawPixel(x, y, color);
    }
}

static void nokiaClearArea(int x0, int y0, int x1, int y1)
{
    for (int x = x0; x <= x1; x++) {
        for (int y = y0; y <= y1; y++) {
            nokiaDrawPixel(x, y, 0);
        }
    }
}

/* ===================== KIEU DU LIEU ===================== */
typedef enum { MODE_ANALOG = 0, MODE_DIGITAL = 1 } DispMode;
typedef enum { ST_SEARCH, ST_OK, ST_LOW, ST_HIGH, ST_LEADOFF } Status;

typedef struct __attribute__((packed)) {
    uint8_t  sync[2];
    uint16_t fs;
    uint16_t n;
    uint16_t bpm;
    uint16_t flags;
    uint16_t data[REC_N];
    uint8_t  chk;
} EcgPacket;

/* ===================== BIEN TOAN CUC ===================== */
static DispMode mode = MODE_ANALOG;
static Status   status = ST_SEARCH;
static uint8_t  alarmType = 0;
static bool     leadOff = false;
static uint16_t bpm = 0;

#define FIFO_SIZE 512
static volatile uint16_t fifo[FIFO_SIZE];
static volatile uint16_t fHead = 0, fTail = 0;

static float    lp, base, env;
static bool     dspInit = false;
static uint32_t sIdx = 0, lastPeak = 0;
static bool     above = false;
static uint16_t rr[4];
static uint8_t  rrN = 0, rrPos = 0;

static int      colX = 0, prevY = WAVE_YC;
static float    acc = 0.0f;
static uint8_t  accN = 0;

#define FLAT_THRESH    30
static bool     noSignal = true;
static bool     noSigShown = false;
static bool     flatSig = true;
static uint16_t winMin = 4095, winMax = 0, winCnt = 0;

static EcgPacket pkt;
static uint16_t  recIdx = 0;

/* ===================== NGAT TIMER 250 Hz (TIM3) ===================== */
void TIM3_IRQHandler(void)
{
    if (TIM3->SR & TIM_SR_UIF) {
        TIM3->SR = (uint16_t)~TIM_SR_UIF;

        HAL_ADC_Start(&hadc1);
        if (HAL_ADC_PollForConversion(&hadc1, 2) == HAL_OK) {
            uint16_t v = (uint16_t)HAL_ADC_GetValue(&hadc1);
            uint16_t next = (uint16_t)((fHead + 1) % FIFO_SIZE);
            if (next != fTail) {
                fifo[fHead] = v;
                fHead = next;
            }
        }
    }
}

static void ecgTimerStart(void)
{
    uint32_t tclk = HAL_RCC_GetPCLK1Freq();
    if ((RCC->CFGR & RCC_CFGR_PPRE1) != 0U) tclk *= 2U;

    __HAL_RCC_TIM3_CLK_ENABLE();
    TIM3->CR1  = 0;
    TIM3->PSC  = (uint16_t)(tclk / 10000UL - 1UL);
    TIM3->ARR  = (uint16_t)(10000UL / FS_HZ - 1UL);
    TIM3->EGR  = TIM_EGR_UG;
    TIM3->SR   = 0;
    TIM3->DIER = TIM_DIER_UIE;
    NVIC_SetPriority(TIM3_IRQn, 1);
    NVIC_EnableIRQ(TIM3_IRQn);
    TIM3->CR1  = TIM_CR1_CEN;
}

static bool fifoPop(uint16_t *v)
{
    if (fTail == fHead) return false;
    *v = fifo[fTail];
    fTail = (uint16_t)((fTail + 1) % FIFO_SIZE);
    return true;
}

/* ===================== XU LY TIN HIEU ===================== */
static void dspReset(void)
{
    dspInit = false;
    sIdx = 0; lastPeak = 0; above = false;
    rrN = 0; rrPos = 0; bpm = 0;
}

static float dspProcess(uint16_t raw)
{
    float d, a, thr;

    if (!dspInit) { lp = base = (float)raw; env = 100.0f; dspInit = true; }

    lp   += 0.4f  * ((float)raw - lp);
    base += 0.01f * (lp - base);
    d = lp - base;
    a = fabsf(d);

    env = (a > env) ? a : env * 0.999f;
    if (env < 60.0f) env = 60.0f;

    sIdx++;
    thr = 0.55f * env;

    if (a > thr) {
        if (!above && (sIdx - lastPeak) > (uint32_t)(FS_HZ * 3 / 10)) {
            if (lastPeak != 0) {
                uint32_t r = sIdx - lastPeak;
                if (r < (uint32_t)(FS_HZ * 2)) {
                    rr[rrPos] = (uint16_t)r;
                    rrPos = (uint8_t)((rrPos + 1) % 4);
                    if (rrN < 4) rrN++;
                    if (rrN >= 2) {
                        uint32_t sum = 0, avg;
                        for (uint8_t i = 0; i < rrN; i++) sum += rr[i];
                        avg = sum / rrN;
                        bpm = (uint16_t)((60UL * FS_HZ) / avg);
                    }
                } else {
                    rrN = 0; rrPos = 0;
                }
            }
            lastPeak = sIdx;
        }
        above = true;
    } else if (a < thr * 0.5f) {
        above = false;
    }

    if (bpm && (sIdx - lastPeak) > (uint32_t)(FS_HZ * 3)) {
        bpm = 0; rrN = 0; rrPos = 0;
    }

    return d / (env * 1.2f);
}

/* ===================== TRANG THAI + COI ===================== */
static void updateStatus(void)
{
    static uint32_t tLoChange = 0;
    static bool     loRaw = false, loStable = false;
    uint32_t now = HAL_GetTick();

    bool r = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_SET) ||
             (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1) == GPIO_PIN_SET);

    if (r != loRaw) { loRaw = r; tLoChange = now; }
    if ((now - tLoChange) >= 200) loStable = loRaw;   /* chi doi khi on dinh 200ms */
    leadOff = loStable;

    if (leadOff)                 { status = ST_LEADOFF; alarmType = 2; }
    else if (bpm == 0)           { status = ST_SEARCH;  alarmType = 0; }
    else if (bpm < BPM_LOW)      { status = ST_LOW;     alarmType = 1; }
    else if (bpm > BPM_HIGH)     { status = ST_HIGH;    alarmType = 1; }
    else                         { status = ST_OK;      alarmType = 0; }
}

static void buzzerUpdate(uint32_t now)
{
    static uint32_t t = 0;
    static uint8_t  on = 0;
    uint32_t period;

    if ((int32_t)(now - muteUntil) < 0 || alarmType == 0) {
        on = 0;
        t = now;
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_RESET);
        return;
    }
    period = (alarmType == 1) ? 150 : 500;
    if (now - t >= period) {
        t = now;
        on ^= 1;
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}

/* ===================== MAN HINH ===================== */
static void drawHeader(bool force)
{
    static char o1[10], o2[10], o3[10];
    char s1[10], s2[10], s3[10];

    if (bpm) snprintf(s1, sizeof s1, "B:%3u", (unsigned)bpm);
    else     snprintf(s1, sizeof s1, "B:---");

    /* Nhan che do hien thi */
    snprintf(s3, sizeof s3, "%s", (mode == MODE_ANALOG) ? "ANA" : "DIG");

    switch (status) {
        case ST_OK:      snprintf(s2, sizeof s2, "OK "); break;
        case ST_LOW:     snprintf(s2, sizeof s2, "LOW"); break;
        case ST_HIGH:    snprintf(s2, sizeof s2, "HI "); break;
        case ST_LEADOFF: snprintf(s2, sizeof s2, "OFF"); break;
        default:         snprintf(s2, sizeof s2, "SRI"); break;
    }

    if (force || strcmp(s1, o1)) { nokiaDrawString(0,  0, s1); strcpy(o1, s1); }
    if (force || strcmp(s3, o3)) { nokiaDrawString(33, 0, s3); strcpy(o3, s3); }
    if (force || strcmp(s2, o2)) { nokiaDrawString(54, 0, s2); strcpy(o2, s2); }
}

static void clearWave(void)
{
    nokiaClearArea(0, WAVE_Y0, LCD_W - 1, WAVE_Y1);
    colX = 0; prevY = WAVE_YC; acc = 0.0f; accN = 0;
    noSigShown = false;
}

static void screenInit(void)
{
    lcdClear();
    // Ve duong gach ngang phan cach header va song
    for (int x = 0; x < LCD_W; x++) nokiaDrawPixel(x, WAVE_Y0 - 1, 1);
    clearWave();
}

static void ecgReset(void)
{
    muteUntil = HAL_GetTick() + MUTE_MS;
    dspReset();
    fTail = fHead;
    recIdx = 0;
    flatSig = true; winMin = 4095; winMax = 0; winCnt = 0;
    screenInit();
    drawHeader(true);
}

static void drawNoSignal(void)
{
    clearWave();
    nokiaDrawString(2, 24, "NO SIGNAL");
    noSigShown = true;
}

static void plotValue(float norm)
{
    int y, xe;

    if (mode == MODE_ANALOG) {
        /* Analog: ve song lien tuc nhu cu */
        if (norm >  1.0f) norm =  1.0f;
        if (norm < -1.0f) norm = -1.0f;
        y = WAVE_YC - (int)(norm * WAVE_HALF);
    } else {
        /* Digital: song vuong 1/0, co tre (hysteresis) de khong bi nhap nhay */
        static uint8_t level = 0;
        float a = fabsf(norm);

        if (a > 0.45f)      level = 1;
        else if (a < 0.25f) level = 0;

        y = level ? (WAVE_YC - WAVE_HALF)    /* muc 1: tren */
                  : (WAVE_YC + WAVE_HALF);   /* muc 0: duoi */
    }

    xe = colX + CLEAR_AHEAD;
    if (xe > LCD_W - 1) xe = LCD_W - 1;
    nokiaClearArea(colX, WAVE_Y0, xe, WAVE_Y1);

    /* Khi doi muc, VLine tu prevY toi y se tao canh thang dung cua song vuong */
    nokiaDrawVLine(colX, prevY, y, 1);

    prevY = y;
    if (++colX >= LCD_W) colX = 0;
}

/* ===================== GHI 10 s + GUI UART ===================== */
static void recordSample(uint16_t raw)
{
    pkt.data[recIdx++] = raw;

    if (recIdx >= REC_N) {
        const uint8_t *b;
        uint8_t sum = 0;

        pkt.sync[0] = 0xAA;
        pkt.sync[1] = 0x55;
        pkt.fs      = FS_HZ;
        pkt.n       = REC_N;
        pkt.bpm     = bpm;
        pkt.flags   = (uint16_t)((leadOff ? 1 : 0) | ((alarmType ? 1 : 0) << 1));

        b = (const uint8_t *)&pkt + 2;
        for (uint16_t i = 0; i < 8 + REC_N * 2; i++) sum = (uint8_t)(sum + b[i]);
        pkt.chk = sum;

        HAL_UART_Transmit(&huart1, (uint8_t *)&pkt, sizeof(EcgPacket), 3000);
        recIdx = 0;
    }
}

/* ===================== NUT BAM ===================== */
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

static void handleButtons(uint32_t now)
{
    static uint8_t  sUp = 0, sDn = 0;
    static uint32_t tUp = 0, tDn = 0;

    if (btnEdge(GPIOA, GPIO_PIN_6, &sUp, &tUp, now)) {
        mode = (mode == MODE_ANALOG) ? MODE_DIGITAL : MODE_ANALOG;
        clearWave();
        drawHeader(true);
    }
    if (btnEdge(GPIOA, GPIO_PIN_8, &sDn, &tDn, now)) {
        ecgReset();
    }
}

/* ===================== API ===================== */
static void ECG_Init(void)
{
    muteUntil = HAL_GetTick() + MUTE_MS;
    ADC_ChannelConfTypeDef adcCh = {0};

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_RESET);
    LCD_BL(1); // Bat den nen man hinh

    lcdInit();

    adcCh.Channel      = ADC_CHANNEL_0;
    adcCh.Rank         = ADC_REGULAR_RANK_1;
    adcCh.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;
    HAL_ADC_ConfigChannel(&hadc1, &adcCh);
    HAL_ADCEx_Calibration_Start(&hadc1);

    screenInit();
    drawHeader(true);

    fHead = fTail = 0;
    ecgTimerStart();
}

static void ECG_Loop(void)
{
    static uint32_t tHeader = 0;
    static uint32_t tRefresh = 0; // Bi?n luu th?i di?m làm tuoi màn hình
    uint32_t now = HAL_GetTick();
    uint16_t raw;

    handleButtons(now);
    updateStatus();

    while (fifoPop(&raw)) {
        float norm;
        lastRaw = raw;

        if (raw < winMin) winMin = raw;
        if (raw > winMax) winMax = raw;
        if (++winCnt >= FS_HZ) {
            flatSig = ((int)(winMax - winMin) < FLAT_THRESH);
            winMin = 4095; winMax = 0; winCnt = 0;
        }
        noSignal = leadOff || flatSig;

        if (leadOff) { dspReset(); norm = 0.0f; }
        else         { norm = dspProcess(raw); }

        recordSample(raw);

        if (noSignal) {
            if (!noSigShown) drawNoSignal();
        } else {
            if (noSigShown) clearWave();
            acc += norm;
            if (++accN >= DISP_DECIM) {
                plotValue(acc / accN);
                acc = 0.0f; accN = 0;
            }
        }
    }

    // CH? LÀM TU?I MÀN HÌNH M?I 30ms (~33 khung hình/giây) Ð? TRÁNH NH?P NHÁY
    if (now - tRefresh >= 30) {
        tRefresh = now;
        nokiaRefresh(); 
    }

    if (now - tHeader >= HEADER_MS) {
        tHeader = now;
        drawHeader(false);
    }

    buzzerUpdate(now);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_USART1_UART_Init();
  MX_SPI1_Init();

  ECG_Init();

  while (1)
  {
    ECG_Loop();
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_ADC1_Init(void)
{
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  ADC_ChannelConfTypeDef sConfig = {0};
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_SPI1_Init(void)
{
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_USART1_UART_Init(void)
{
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10|GPIO_PIN_11, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_11;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void Error_Handler(void)
{
  __disable_irq();
  while (1) {}
}