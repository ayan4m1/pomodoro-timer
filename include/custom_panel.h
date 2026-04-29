#pragma once

#define LCD_SPI_HOST SPI_3
// #define LCD_BCKL_ON_LEVEL 1
#define LCD_PIN_NUM_MOSI 39
#define LCD_PIN_NUM_CLK 40
#define LCD_PIN_NUM_CS 41
#define LCD_PIN_NUM_DC 45
#define LCD_PIN_NUM_RST 21
#define LCD_PIN_NUM_BCKL 38
#define LCD_INIT esp_lcd_new_panel_st7789
#define LCD_HRES 135
#define LCD_VRES 240
#define LCD_HEIGHT LCD_HRES
#define LCD_WIDTH LCD_VRES
#define LCD_COLOR_SPACE LCD_COLOR_BGR
#define LCD_CLOCK_HZ (40 * 1000 * 1000)
#define LCD_GAP_X 0
#define LCD_GAP_Y 0
#define LCD_MIRROR_X 0
#define LCD_MIRROR_Y 0
#define LCD_INVERT_COLOR 0
#define LCD_BIT_DEPTH 16
#define LCD_SWAP_XY 0
#define BUTTON_MASK (BUTTON_PIN(11) | BUTTON_PIN(12))
#define BUTTON_ON_LEVEL 1