/* Driver Nokia 5110 (PCD8544) qua SPI1
 *   PA1 BL | PA2 SCE(CS) | PA3 DC | PA4 RST | PA5 SCK | PA7 MOSI */
#include "main.h"
#include <string.h>
#include "nokia5110.h"
#include "font5x7.h"

extern SPI_HandleTypeDef hspi1;

#define LCD_CS(x)   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define LCD_DC(x)   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define LCD_RST(x)  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define LCD_BL(x)   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)

static uint8_t lcd_buffer[LCD_W * 6];   /* 84 cot x 6 hang (moi hang 8 pixel) */

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

void Nokia_Backlight(uint8_t on)
{
    LCD_BL(on);
}

void Nokia_Init(void)
{
    LCD_CS(1);
    LCD_RST(0); HAL_Delay(20);
    LCD_RST(1); HAL_Delay(50);

    nokiaWriteCmd(0x21); /* Extended instruction set */
    nokiaWriteCmd(0xC8); /* Vop (contrast): thu 0xC0-0xCF neu man hinh mo */
    nokiaWriteCmd(0x06); /* Temperature coefficient */
    nokiaWriteCmd(0x13); /* Bias 1:48 */
    nokiaWriteCmd(0x20); /* Standard instruction set, horizontal addressing */
    nokiaWriteCmd(0x0C); /* Normal display (0x0D: dao nguoc) */
}

void Nokia_Refresh(void)
{
    nokiaWriteCmd(0x80); /* X = 0 */
    nokiaWriteCmd(0x40); /* Y = 0 */
    LCD_DC(1); LCD_CS(0);
    lcdWrite(lcd_buffer, sizeof(lcd_buffer));
    LCD_CS(1);
}

void Nokia_Clear(void)
{
    memset(lcd_buffer, 0, sizeof(lcd_buffer));
}

void Nokia_DrawChar(uint8_t x, uint8_t row, char ch)
{
    const uint8_t *g = Font5x7_Glyph(ch);
    for (uint8_t i = 0; i < 5; i++) {
        if ((x + i) < LCD_W && row < 6) {
            lcd_buffer[x + i + (row * LCD_W)] = g[i];
        }
    }
}

void Nokia_DrawString(uint8_t x, uint8_t row, const char *s)
{
    while (*s) {
        Nokia_DrawChar(x, row, *s++);
        x += 6;
        if (x > LCD_W - 6) break;
    }
}

void Nokia_DrawPixel(int x, int y, uint8_t color)
{
    if (x < 0 || x >= LCD_W || y < 0 || y >= LCD_H) return;
    if (color) lcd_buffer[x + ((y / 8) * LCD_W)] |=  (1 << (y % 8));
    else       lcd_buffer[x + ((y / 8) * LCD_W)] &= ~(1 << (y % 8));
}

void Nokia_DrawVLine(int x, int y0, int y1, uint8_t color)
{
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    for (int y = y0; y <= y1; y++) Nokia_DrawPixel(x, y, color);
}

void Nokia_ClearArea(int x0, int y0, int x1, int y1)
{
    for (int x = x0; x <= x1; x++)
        for (int y = y0; y <= y1; y++)
            Nokia_DrawPixel(x, y, 0);
}
