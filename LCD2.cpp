// LCD2.cpp
// Second ST7735 display driver — portrait mode (128×160 pixels).
//
// Hardware wiring:
//   PA26 (PINCM 58) — LCD2 chip-select (active-low)
//   PB6             — DC (shared with LCD1; command=low, data=high)
//   PB8             — SPI1 MOSI (shared)
//   PB9             — SPI1 SCLK (shared)
//   PA12            — Hardware reset (shared; LCD1 init already pulses this)
//
// SPI1 is initialized by LCD1's SPI_Init() call inside ST7735_InitR().
// LCD2_Init() only configures PA26 as a GPIO output, then sends the ST7735
// command sequence to LCD2 via its own chip-select.  The two displays are
// completely independent in software: only the CS line distinguishes them.
//
// Kaitlyn Chen and Keerthana Mangalpally
// ECE319H Lab 9, Spring 2026

#include <stdint.h>
#include <ti/devices/msp/msp.h>
#include "LCD2.h"
#include "game.h"         // for S_TITLE, S_GAMEPLAY, etc.
#include "ST7735.h"       // for ST7735_DrawString on LCD2 via shared SPI
#include "../inc/Clock.h"
#include "../inc/SPI.h"

// ── Pin definitions ───────────────────────────────────────────────────────
#define LCD2_CS_PINCM  59         // PINCM index for PA27
#define LCD2_CS_BIT    (1UL<<27)  // PA27 bitmask

// ── ST7735 command bytes ──────────────────────────────────────────────────
#define ST7735_SWRESET  0x01
#define ST7735_SLPOUT   0x11
#define ST7735_NORON    0x13
#define ST7735_INVOFF   0x20
#define ST7735_DISPON   0x29
#define ST7735_CASET    0x2A
#define ST7735_RASET    0x2B
#define ST7735_RAMWR    0x2C
#define ST7735_MADCTL   0x36
#define ST7735_COLMOD   0x3A
#define ST7735_FRMCTR1  0xB1
#define ST7735_FRMCTR2  0xB2
#define ST7735_FRMCTR3  0xB3
#define ST7735_INVCTR   0xB4
#define ST7735_PWCTR1   0xC0
#define ST7735_PWCTR2   0xC1
#define ST7735_PWCTR3   0xC2
#define ST7735_PWCTR4   0xC3
#define ST7735_PWCTR5   0xC4
#define ST7735_VMCTR1   0xC5
#define ST7735_GMCTRP1  0xE0
#define ST7735_GMCTRN1  0xE1

// Portrait orientation for BLACKTAB:
//   MADCTL_MX (0x40) | MADCTL_MY (0x80) | MADCTL_RGB (0x00) = 0xC0
#define LCD2_MADCTL_PORTRAIT  0xC0

// ── Low-level SPI helpers ─────────────────────────────────────────────────
// Each function asserts PA26 (LCD2_CS), uses PB6 (DC) shared with LCD1,
// then deasserts PA26.  LCD1 (PA27) remains deasserted throughout, so it
// ignores all traffic here.

static void LCD2_Cmd(uint8_t cmd){
    SPI_OutCommand(cmd);
}

static void LCD2_Data(uint8_t dat){
    SPI_OutData(dat);
}

static void LCD2_Begin(void){
    SPI_SelectDisplay(SPI_DISPLAY_LCD2);
}

static void LCD2_End(void){
    SPI_SelectDisplay(SPI_DISPLAY_LCD1);
}

// ── Drawing primitives ────────────────────────────────────────────────────

// Set the pixel address window for subsequent RAMWR data.
static void LCD2_SetWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1){
    LCD2_Cmd(ST7735_CASET);
    LCD2_Data(0x00); LCD2_Data(x0);
    LCD2_Data(0x00); LCD2_Data(x1);
    LCD2_Cmd(ST7735_RASET);
    LCD2_Data(0x00); LCD2_Data(y0);
    LCD2_Data(0x00); LCD2_Data(y1);
    LCD2_Cmd(ST7735_RAMWR);
}

// Fill a rectangle with a solid 16-bit color.
static void LCD2_FillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint16_t color){
    if(x >= LCD2_WIDTH || y >= LCD2_HEIGHT) return;
    if((uint16_t)x + w > LCD2_WIDTH)  w = LCD2_WIDTH  - x;
    if((uint16_t)y + h > LCD2_HEIGHT) h = LCD2_HEIGHT - y;
    LCD2_SetWindow(x, y, x+w-1, y+h-1);
    uint8_t hi = (uint8_t)(color >> 8), lo = (uint8_t)(color & 0xFF);
    uint32_t count = (uint32_t)w * h;
    while(count--){
        LCD2_Data(hi);
        LCD2_Data(lo);
    }
}

void LCD2_FillScreen(uint16_t color){
    LCD2_FillRect(0, 0, LCD2_WIDTH, LCD2_HEIGHT, color);
}

// ── Initialization ────────────────────────────────────────────────────────

void LCD2_Init(void){
    // Configure PA26 as GPIO output, idle-high (CS deasserted)
    // IOMUX->SECCFG.PINCM[LCD2_CS_PINCM] = 0x00000081;  // GPIO function, output
    // GPIOA->DOE31_0    |= LCD2_CS_BIT;
    // GPIOA->DOUTSET31_0 = LCD2_CS_BIT;   // CS = 1 (idle)

    // SPI1 is already configured and running (LCD1 init started it).
    // Brief delay for power settling, then run software reset on LCD2.
    Clock_Delay1ms(20);
    LCD2_Begin();

    // ── ST7735R init sequence (BLACKTAB, portrait) ────────────────────────
    // Mirrors the Rcmd1 + Rcmd2red + Rcmd3 sequences from ST7735.cpp,
    // but uses LCD2_Cmd/LCD2_Data so PA26 is asserted instead of PA27.

    // Software reset
    LCD2_Cmd(ST7735_SWRESET); Clock_Delay1ms(150);
    // Exit sleep
    LCD2_Cmd(ST7735_SLPOUT);  Clock_Delay1ms(255);

    // Frame rate control (normal / idle / partial modes)
    LCD2_Cmd(ST7735_FRMCTR1);
        LCD2_Data(0x01); LCD2_Data(0x2C); LCD2_Data(0x2D);
    LCD2_Cmd(ST7735_FRMCTR2);
        LCD2_Data(0x01); LCD2_Data(0x2C); LCD2_Data(0x2D);
    LCD2_Cmd(ST7735_FRMCTR3);
        LCD2_Data(0x01); LCD2_Data(0x2C); LCD2_Data(0x2D);
        LCD2_Data(0x01); LCD2_Data(0x2C); LCD2_Data(0x2D);

    // Display inversion control: no inversion
    LCD2_Cmd(ST7735_INVCTR); LCD2_Data(0x07);

    // Power control
    LCD2_Cmd(ST7735_PWCTR1); LCD2_Data(0xA2); LCD2_Data(0x02); LCD2_Data(0x84);
    LCD2_Cmd(ST7735_PWCTR2); LCD2_Data(0xC5);
    LCD2_Cmd(ST7735_PWCTR3); LCD2_Data(0x0A); LCD2_Data(0x00);
    LCD2_Cmd(ST7735_PWCTR4); LCD2_Data(0x8A); LCD2_Data(0x2A);
    LCD2_Cmd(ST7735_PWCTR5); LCD2_Data(0x8A); LCD2_Data(0xEE);
    LCD2_Cmd(ST7735_VMCTR1); LCD2_Data(0x0E);

    // No display inversion
    LCD2_Cmd(ST7735_INVOFF);

    // 16-bit color mode
    LCD2_Cmd(ST7735_COLMOD); LCD2_Data(0x05);

    // Memory access control — portrait orientation, BLACKTAB color order
    LCD2_Cmd(ST7735_MADCTL); LCD2_Data(LCD2_MADCTL_PORTRAIT);

    // Gamma correction tables (Rcmd3 values)
    LCD2_Cmd(ST7735_GMCTRP1);
        LCD2_Data(0x02); LCD2_Data(0x1C); LCD2_Data(0x07); LCD2_Data(0x12);
        LCD2_Data(0x37); LCD2_Data(0x32); LCD2_Data(0x29); LCD2_Data(0x2D);
        LCD2_Data(0x29); LCD2_Data(0x25); LCD2_Data(0x2B); LCD2_Data(0x39);
        LCD2_Data(0x00); LCD2_Data(0x01); LCD2_Data(0x03); LCD2_Data(0x10);
    LCD2_Cmd(ST7735_GMCTRN1);
        LCD2_Data(0x03); LCD2_Data(0x1D); LCD2_Data(0x07); LCD2_Data(0x06);
        LCD2_Data(0x2E); LCD2_Data(0x2C); LCD2_Data(0x29); LCD2_Data(0x2D);
        LCD2_Data(0x2E); LCD2_Data(0x2E); LCD2_Data(0x37); LCD2_Data(0x3F);
        LCD2_Data(0x00); LCD2_Data(0x00); LCD2_Data(0x02); LCD2_Data(0x10);

    // Column / row address limits (128×160 portrait)
    LCD2_Cmd(ST7735_CASET);
        LCD2_Data(0x00); LCD2_Data(0x00);
        LCD2_Data(0x00); LCD2_Data(0x7F);   // 0 to 127
    LCD2_Cmd(ST7735_RASET);
        LCD2_Data(0x00); LCD2_Data(0x00);
        LCD2_Data(0x00); LCD2_Data(0x9F);   // 0 to 159

    // Normal display on, then display on
    LCD2_Cmd(ST7735_NORON);  Clock_Delay1ms(10);
    LCD2_Cmd(ST7735_DISPON); Clock_Delay1ms(100);

    // Clear to black twice to wipe any power-up garbage before normal use.
    LCD2_FillScreen(LCD2_BLACK);
    LCD2_FillScreen(LCD2_BLACK);
    LCD2_End();
}


// ── Score display — 7-segment style digits ────────────────────────────────
//
// Each digit occupies a 20×36 pixel cell.
// Three digits are centered on the 128-pixel-wide portrait display:
//   total width = 3*20 + 2*4 (gap) = 68 px → start x = (128-68)/2 = 30
//
// Segment positions (relative to digit's top-left corner at (dx, dy)):
//   SEG_A (top):          FillRect(dx+2, dy,    16, 3, ...)
//   SEG_B (upper-right):  FillRect(dx+17,dy+3,   3,15, ...)
//   SEG_C (lower-right):  FillRect(dx+17,dy+21,  3,15, ...)
//   SEG_D (bottom):       FillRect(dx+2, dy+33, 16, 3, ...)
//   SEG_E (lower-left):   FillRect(dx,   dy+21,  3,15, ...)
//   SEG_F (upper-left):   FillRect(dx,   dy+3,   3,15, ...)
//   SEG_G (middle):       FillRect(dx+2, dy+18, 16, 3, ...)
//
// Segment mask bit encoding: bit0=A, 1=B, 2=C, 3=D, 4=E, 5=F, 6=G
static const uint8_t kSegMask[10] = {
    0x3F,  // 0: A B C D E F
    0x06,  // 1: B C
    0x5B,  // 2: A B D E G
    0x4F,  // 3: A B C D G
    0x66,  // 4: B C F G
    0x6D,  // 5: A C D F G
    0x7D,  // 6: A C D E F G
    0x07,  // 7: A B C
    0x7F,  // 8: all
    0x6F,  // 9: A B C D F G
};

// Draw one 7-segment digit at pixel position (dx, dy).
// on_color: color for lit segments; bg_color: color for dark segments.
static void LCD2_DrawDigit(uint8_t dx, uint8_t dy,
                           uint8_t digit,
                           uint16_t on_color, uint16_t bg_color){
    uint8_t m = kSegMask[digit % 10];
    // SEG_A
    LCD2_FillRect(dx+2,  dy,    16, 3,  (m & 0x01) ? on_color : bg_color);
    // SEG_B
    LCD2_FillRect(dx+17, dy+3,   3, 15, (m & 0x02) ? on_color : bg_color);
    // SEG_C
    LCD2_FillRect(dx+17, dy+21,  3, 15, (m & 0x04) ? on_color : bg_color);
    // SEG_D
    LCD2_FillRect(dx+2,  dy+33, 16, 3,  (m & 0x08) ? on_color : bg_color);
    // SEG_E
    LCD2_FillRect(dx,    dy+21,  3, 15, (m & 0x10) ? on_color : bg_color);
    // SEG_F
    LCD2_FillRect(dx,    dy+3,   3, 15, (m & 0x20) ? on_color : bg_color);
    // SEG_G
    LCD2_FillRect(dx+2,  dy+18, 16, 3,  (m & 0x40) ? on_color : bg_color);
}

// Digit area: 3 digits starting at x=30, y=22.  Each 20px wide, 4px gap.
#define DIGIT_X0  30
#define DIGIT_Y0  22
#define DIGIT_W   20
#define DIGIT_GAP  4
#define SCORE_PANEL_H 62
#define LIVES_PANEL_Y 62
#define LIVES_PANEL_H 26

void LCD2_ShowScore(uint32_t score){
    LCD2_Begin();
    LCD2_FillRect(0, 0, LCD2_WIDTH, SCORE_PANEL_H, LCD2_BLACK);

    // Score digits (y=22, 3-digit display always shown)
    uint8_t d2 =  score % 10;
    uint8_t d1 = (score / 10)  % 10;
    uint8_t d0 = (score / 100) % 10;

    uint16_t seg_on  = LCD2_YELLOW;
    uint16_t seg_off = LCD2_BLACK;
    uint8_t x0 = DIGIT_X0;
    uint8_t x1 = DIGIT_X0 + DIGIT_W + DIGIT_GAP;
    uint8_t x2 = DIGIT_X0 + 2*(DIGIT_W + DIGIT_GAP);

    LCD2_DrawDigit(x0, DIGIT_Y0, d0, seg_on, seg_off);
    LCD2_DrawDigit(x1, DIGIT_Y0, d1, seg_on, seg_off);
    LCD2_DrawDigit(x2, DIGIT_Y0, d2, seg_on, seg_off);
    LCD2_End();
}

// ── Lives display — colored indicator squares ─────────────────────────────
//
// Three 14×14 squares centered in a 30-pixel-tall band at y=64.
// Red = life remaining; dark grey = life lost.

#define LIVES_Y0    64
#define LIVES_SZ    14    // square side length
#define LIVES_GAP    6    // gap between squares
// Center 3 squares: total = 3*14 + 2*6 = 54px; start x = (128-54)/2 = 37

void LCD2_ShowLives(uint8_t lives){
    LCD2_Begin();
    // Redraw the full lives panel so stale pixels never linger.
    LCD2_FillRect(0, LIVES_PANEL_Y, LCD2_WIDTH, LIVES_PANEL_H, LCD2_BLACK);

    static const uint8_t xs[3] = {37, 37+LIVES_SZ+LIVES_GAP, 37+2*(LIVES_SZ+LIVES_GAP)};
    for(int i = 0; i < 3; i++){
        uint16_t color = (i < (int)lives) ? LCD2_RED : LCD2_DARKGREY;
        LCD2_FillRect(xs[i], LIVES_Y0, LIVES_SZ, LIVES_SZ, color);
    }
    LCD2_End();
}

// ── Reaction face ─────────────────────────────────────────────────────────
//
// A cartoon face drawn entirely with rectangles.
// Face "circle": large yellow rect with black corner squares to fake rounding.
// Eyes: two small dark squares.
// Mouth: three rect segments forming a smile, frown, or flat line.
//
// Face bounding box: x=36, y=90, w=56, h=56 → right edge x=91, bottom y=145

#define FACE_X   36
#define FACE_Y   90
#define FACE_W   56
#define FACE_H   56
#define FACE_CRN 10   // corner square size (masks the rectangle's corners)

// Helper: draw the face oval + eyes, with a specified face color.
static void DrawFaceBase(uint16_t face_color, uint16_t bg_color){
    // Fill face rectangle
    LCD2_FillRect(FACE_X, FACE_Y, FACE_W, FACE_H, face_color);
    // Mask corners to approximate a circle
    LCD2_FillRect(FACE_X,              FACE_Y,              FACE_CRN, FACE_CRN, bg_color);
    LCD2_FillRect(FACE_X+FACE_W-FACE_CRN, FACE_Y,          FACE_CRN, FACE_CRN, bg_color);
    LCD2_FillRect(FACE_X,              FACE_Y+FACE_H-FACE_CRN, FACE_CRN, FACE_CRN, bg_color);
    LCD2_FillRect(FACE_X+FACE_W-FACE_CRN, FACE_Y+FACE_H-FACE_CRN, FACE_CRN, FACE_CRN, bg_color);
    // Eyes: two 8×8 dark squares
    LCD2_FillRect(FACE_X+10, FACE_Y+14, 8, 8, LCD2_BLACK);   // left eye
    LCD2_FillRect(FACE_X+38, FACE_Y+14, 8, 8, LCD2_BLACK);   // right eye
}

// Mouth shapes — three rect segments:
//   smile:   left-up / center-down / right-up  (∪ shape)
//   frown:   left-down / center-up / right-down (∩ shape)
//   neutral: single flat rectangle
static void DrawSmile(uint16_t face_color){
    // Erase mouth area first
    LCD2_FillRect(FACE_X+8, FACE_Y+33, FACE_W-16, 12, face_color);
    LCD2_FillRect(FACE_X+10, FACE_Y+33, 10, 5, LCD2_BLACK);  // left
    LCD2_FillRect(FACE_X+20, FACE_Y+38,  16, 5, LCD2_BLACK); // center (lower)
    LCD2_FillRect(FACE_X+36, FACE_Y+33, 10, 5, LCD2_BLACK);  // right
}

static void DrawFrown(uint16_t face_color){
    LCD2_FillRect(FACE_X+8, FACE_Y+33, FACE_W-16, 12, face_color);
    LCD2_FillRect(FACE_X+10, FACE_Y+38, 10, 5, LCD2_BLACK);  // left (lower)
    LCD2_FillRect(FACE_X+20, FACE_Y+33,  16, 5, LCD2_BLACK); // center (higher)
    LCD2_FillRect(FACE_X+36, FACE_Y+38, 10, 5, LCD2_BLACK);  // right (lower)
}

static void DrawNeutral(uint16_t face_color){
    LCD2_FillRect(FACE_X+8, FACE_Y+33, FACE_W-16, 12, face_color);
    LCD2_FillRect(FACE_X+10, FACE_Y+36, FACE_W-20, 5, LCD2_BLACK);  // flat line
}

// Closed/squint eyes for "sleeping" expression
static void DrawSleepyEyes(uint16_t face_color){
    // Redraw eye areas, replace solid squares with horizontal slits
    LCD2_FillRect(FACE_X+10, FACE_Y+14, 8, 8, face_color);   // clear left eye
    LCD2_FillRect(FACE_X+38, FACE_Y+14, 8, 8, face_color);   // clear right eye
    LCD2_FillRect(FACE_X+10, FACE_Y+17, 8, 3, LCD2_BLACK);   // left slit
    LCD2_FillRect(FACE_X+38, FACE_Y+17, 8, 3, LCD2_BLACK);   // right slit
}

void LCD2_ShowReaction(uint8_t state){
    LCD2_Begin();
    uint16_t bg_color = LCD2_BLACK;
    // Redraw the full bottom panel every time so old content cannot linger.
    LCD2_FillRect(0, 88, LCD2_WIDTH, LCD2_HEIGHT - 88, bg_color);

    uint16_t face_color;

    switch(state){
        case S_GAMEPLAY:
            // Show instructions text instead of face (score/lives are above this area)
            // SPI is directed to LCD2 here, so ST7735_DrawString draws on LCD2.
            ST7735_DrawString(1, 9,  (char*)"JOY:  move blade", (int16_t)LCD2_CYAN);
            ST7735_DrawString(1, 10, (char*)"TOP:  slash fruit",(int16_t)LCD2_YELLOW);
            ST7735_DrawString(1, 11, (char*)"BOT:  pause",      (int16_t)LCD2_WHITE);
            ST7735_DrawString(1, 12, (char*)"3 lives total",    (int16_t)LCD2_WHITE);
            ST7735_DrawString(1, 13, (char*)"Avoid the bombs!", (int16_t)LCD2_RED);
            ST7735_DrawString(2, 14, (char*)"- GOOD LUCK! -",   (int16_t)LCD2_GREEN);
            break;

        case S_GAMEOVER:
            // Sad red face on game over
            face_color = LCD2_RED;
            DrawFaceBase(face_color, bg_color);
            DrawFrown(face_color);
            break;

        case S_PAUSED:
            // Sleepy yellow face while paused
            face_color = LCD2_YELLOW;
            DrawFaceBase(face_color, bg_color);
            DrawSleepyEyes(face_color);
            DrawNeutral(face_color);
            // "Zzz" dots above face to indicate sleep
            LCD2_FillRect(FACE_X + FACE_W + 2, FACE_Y,      4, 4, LCD2_WHITE);
            LCD2_FillRect(FACE_X + FACE_W + 8, FACE_Y - 5,  5, 5, LCD2_WHITE);
            LCD2_FillRect(FACE_X + FACE_W +15, FACE_Y -10,  6, 6, LCD2_WHITE);
            break;

        case S_TITLE:
        default:
            // Neutral white face on title/other screens
            face_color = LCD2_WHITE;
            DrawFaceBase(face_color, bg_color);
            DrawNeutral(face_color);
            break;
    }
    LCD2_End();
}
