#ifndef NOKIA5110_H
#define NOKIA5110_H

#include <stdint.h>

#define LCD_W   84
#define LCD_H   48

void Nokia_Init(void);
void Nokia_Backlight(uint8_t on);
void Nokia_Refresh(void);
void Nokia_Clear(void);
void Nokia_DrawChar(uint8_t x, uint8_t row, char ch);     /* row: 0..5 */
void Nokia_DrawString(uint8_t x, uint8_t row, const char *s);
void Nokia_DrawPixel(int x, int y, uint8_t color);
void Nokia_DrawVLine(int x, int y0, int y1, uint8_t color);
void Nokia_ClearArea(int x0, int y0, int x1, int y1);

#endif
