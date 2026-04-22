// LCD2.h
// Driver for the second ST7735 display (portrait orientation, 128x160).
// Shares SPI1 (PB8=MOSI, PB9=SCLK, PA13=DC, PB15=RESET) with LCD1.
// LCD1 chip-select stays on PB6, LCD2 chip-select is PA26 (PINCM index 58), active-low.
// LCD2 is vertical — used for score, lives, and reaction faces.
//
// Kaitlyn Chen and Keerthana Mangalpally
// ECE319H Lab 9, Spring 2026

#ifndef LCD2_H
#define LCD2_H
#include <stdint.h>

// ── Color constants (16-bit RGB565, same encoding as ST7735) ─────────────
#define LCD2_BLACK    0x0000
#define LCD2_WHITE    0xFFFF
#define LCD2_RED      0x001F
#define LCD2_GREEN    0x07E0
#define LCD2_BLUE     0xF800
#define LCD2_CYAN     0xFFE0
#define LCD2_YELLOW   0x07FF
#define LCD2_MAGENTA  0xF81F
#define LCD2_ORANGE   0x041F
#define LCD2_DARKGREY 0x39E7

// ── LCD2 display dimensions (portrait mode) ───────────────────────────────
#define LCD2_WIDTH   128
#define LCD2_HEIGHT  160

// ── Public API ────────────────────────────────────────────────────────────

// Initialize LCD2 hardware (PA26 GPIO + ST7735 init sequence, portrait mode).
// Must be called AFTER ST7735_InitR/InitPrintf (which sets up SPI1).
void LCD2_Init(void);

// Fill the entire display with a solid color.
void LCD2_FillScreen(uint16_t color);

// Display the current score as large 7-segment-style digits.
// score: 0 to 9999 (values >= 1000 show 4 digits).
void LCD2_ShowScore(uint32_t score);

// Display lives remaining as colored indicator squares (3 squares max).
// lives: 0-3 (0 = all grey, 3 = all red).
void LCD2_ShowLives(uint8_t lives);

// Draw a cat sprite in the bottom panel (y=88-160), centered horizontally.
// The caller (game.cpp) selects the sprite to avoid duplicating rodata here.
void LCD2_ShowReaction(const unsigned short *img, int16_t w, int16_t h);

// Display the current difficulty level (0-9) at the bottom of the portrait display.
// Call during gameplay whenever the level changes.
void LCD2_ShowLevel(uint8_t level);

// Show a "PAUSED" indicator in the cat panel (replaces sleeping_cat sprite).
void LCD2_ShowPaused(void);

#endif // LCD2_H
