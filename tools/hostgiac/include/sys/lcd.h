// hostgiac shim for CEdev <sys/lcd.h>: an off-screen buffer
#pragma once
#include <stdint.h>
#define LCD_WIDTH 320
#define LCD_HEIGHT 240
#define LCD_SIZE (LCD_WIDTH*LCD_HEIGHT*2)
#ifdef __cplusplus
extern "C" {
#endif
extern uint16_t hostgiac_lcd_Ram[LCD_WIDTH*LCD_HEIGHT];
#ifdef __cplusplus
}
#endif
#define lcd_Ram ((void*)hostgiac_lcd_Ram)
