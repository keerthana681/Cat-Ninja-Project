// game.cpp
// Cat Ninja – Valvano indexed-FSM game engine
// Kaitlyn Chen and Keerthana Mangalpally
// ECE319H Lab 9, Spring 2026

#include "game.h"
#include "ST7735.h"
#include "Switch.h"
#include "LED.h"
#include "Sound.h"
#include "images/images.h"
#include "JoyStick.h"
#include "LCD2.h"
#include "../inc/SPI.h"
#include <stdlib.h>

extern "C" {
    #include "ADC.h"
}

// ── Sprite tables ──────────────────────────────────────────────────────────
// Even index = whole, odd index = cut/exploded
static const unsigned short* selection[10] = {
    wholestrawberry,  cutstrawberry,
    wholewatermelon,  cutwatermelon,
    wholepineapple,   cutpineapple,
    wholepomegranate, cutpomegranate,
    bomb,             boom
};
typedef struct { int16_t xp, yp; } FruitDim;
static const FruitDim P[10] = {
    {32,32},{51,32},   // 0-1  strawberry
    {38,32},{35,35},   // 2-3  watermelon
    {32,63},{34,38},   // 4-5  pineapple
    {32,32},{42,31},   // 6-7  pomegranate
    {32,48},{69,59}    // 8-9  bomb/boom
};
static const unsigned short* TitleLogos[1] = {titlecat4};
static const int16_t TitleLogoW[1] = { 58 };
static const int16_t TitleLogoH[1] = { 63 };

// ── Fruit types & timing constants ────────────────────────────────────────
#define MAX_FRUITS        5
#define FRUIT_STRAWBERRY  0
#define FRUIT_WATERMELON  2
#define FRUIT_PINEAPPLE   4
#define FRUIT_POMEGRANATE 6
#define FRUIT_BOMB        8
#define FRUIT_BOOM        9
#define EXPLODE_FRAMES    45   // 1.5 s bomb explosion
#define SLICE_FRAMES      10   // bomb-only countdown uses deathTimer
#define POMO_RESLICE      8    // frames until pomegranate can be re-sliced
#define SLOWMO_FRAMES     90   // 3 s slow-motion

typedef struct {
    int16_t  x, y;          // DrawBitmap: y = bottom anchor
    int16_t  vx, vy;
    uint8_t  type;          // index into selection[] / P[]
    bool     active;
    bool     sliced;        // currently showing cut state
    bool     wasVisible;    // only count a miss after the fruit has entered the screen
    uint8_t  hitCount;      // total hits (miss-penalty and pom tracking)
    uint8_t  deathTimer;    // countdown to removal
    uint8_t  slicedTimer;   // pomegranate: frames until re-sliceable
    const unsigned short* image;
} Fruit_t;

static Fruit_t Fruits[MAX_FRUITS];

// ── Game-wide state ────────────────────────────────────────────────────────
uint32_t          S;
volatile uint32_t Semaphore;
uint8_t           CurrentLanguage = LANG_ENGLISH;

static uint32_t PreviousState   = 0xFF;
static uint32_t Score           = 0;
static uint32_t HighScore       = 0;
static uint8_t  Lives           = 3;
static const uint8_t MENU_NONE  = 0xFF;
static uint32_t LCD2ScoreShadow = 0xFFFFFFFF;
static uint8_t  LCD2LivesShadow = 0xFF;
static uint8_t  LCD2LevelShadow = 0xFF;
static const uint8_t LCD2Enabled = 1;
static uint8_t  LCD2MsgTimer   = 0;

// Menu cursor shadow state
static uint8_t prevSelection    = 0xFF;
static uint8_t settingsSelection = 0xFF;
static uint8_t languageSelection = 0xFF;
static uint8_t titleLogoFrame   = 0;
static uint8_t titleLogoTimer   = 0;
static uint8_t titleCursor      = MENU_NONE;
static uint8_t settingsCursor   = MENU_NONE;
static uint8_t languageCursor   = MENU_NONE;

// Joystick cache & slow-mo
static uint32_t JoyX = 2048, JoyY = 2048;
static uint16_t SlowmoTimer = 0;
static uint8_t  SlowmoFrame = 0;
static uint8_t  GravCounter = 0;

// Display constants
#define LCD1_WIDTH   160
#define LCD1_HEIGHT  128
#define LIGHT_GREY   0xC618u   // RGB565 (192,192,192)
// Title screen layout (landscape 160×128):
//   y= 0-25 : "Cat Ninja" title text (catninjatitle 134×25, bottom anchor y=25)
//   y=26-92 : titlecat sprite (bottom anchor TITLE_SPRITE_Y=92, max h=64)
//   y=110+  : INFO / SETTINGS / PLAY buttons (row 11)
#define TITLE_TEXT_Y     25   // bottom anchor of catninjatitle
#define TITLE_SPRITE_Y   92   // bottom anchor of titlecat animation
#define TITLE_BUTTON_ROW 11   // ST7735 text row for menu buttons

// ── Cursor ────────────────────────────────────────────────────────────────
typedef struct {
    int16_t x, y;
    uint8_t radius;
} Cursor_t;
static Cursor_t  BladeCursor;
static uint16_t  CursorColor = ST7735_CYAN;
static const int16_t CURSOR1_W = 15;
static const int16_t CURSOR1_H = 15;
static const int16_t CURSOR2_W = 18;
static const int16_t CURSOR2_H = 19;
static const int16_t CURSOR_MAX_HALF_W = CURSOR2_W / 2;
static const int16_t CURSOR_MAX_HALF_H = CURSOR2_H / 2;

// ── Forward declarations ───────────────────────────────────────────────────
static void Title_RenderButton(uint8_t slot, uint8_t selected);
static void Settings_RenderOption(uint8_t row, const char* label, uint8_t selected);
static void Language_RenderOption(uint8_t row, const char* label, uint8_t selected);
static void LCD2_UpdateGameplayStatus(void);
static void LCD2_ShowEventMsg(const char* msg, uint16_t color, uint8_t frames);
static void Gameplay_DrawBorder(void);
static void Trail_Init(void);
static void Trail_Update(void);
static void Fruits_Draw(void);
static void Fruit_Init(void);
static void Fruit_Update(void);
static void Collision_Check(void);
static void Title_DrawLogo(uint8_t frame);
static void Title_AnimateLogo(void);
static void DrawBitmapMasked(int16_t x, int16_t y, const uint16_t *image, int16_t w, int16_t h);
static uint8_t DifficultyLevel(void);

// ── Inline helpers ─────────────────────────────────────────────────────────
static const char* lang(const char* eng, const char* esp){
    return (CurrentLanguage == LANG_SPANISH) ? esp : eng;
}
static void DrawSep(int16_t y, uint16_t color){
    ST7735_DrawFastHLine(0, y, LCD1_WIDTH, color);
}
static void LCD1_Select(void){ SPI_SelectDisplay(SPI_DISPLAY_LCD1); }
static void LCD2_Sel(void)   { SPI_SelectDisplay(SPI_DISPLAY_LCD2); }

static int16_t Clamp16(int16_t v, int16_t lo, int16_t hi){
    return (v < lo) ? lo : (v > hi) ? hi : v;
}
static int16_t Abs16(int16_t v){ return v < 0 ? -v : v; }
static int16_t MapAxis(uint32_t s, int16_t outMax){
    uint32_t sc = (s * (uint32_t)outMax) / 4095U;
    return (int16_t)(sc > (uint32_t)outMax ? (uint32_t)outMax : sc);
}

// ── LCD2 helpers ───────────────────────────────────────────────────────────
static void LCD2_UpdateGameplayStatus(void){
    if(!LCD2Enabled){ LCD1_Select(); return; }
    uint8_t lv = DifficultyLevel();
    bool scoreChanged = (LCD2ScoreShadow != Score || LCD2LivesShadow != Lives);
    bool levelChanged = (LCD2LevelShadow != lv);
    if(!scoreChanged && !levelChanged){ LCD1_Select(); return; }
    if(scoreChanged){
        LCD2_ShowScore(Score);
        LCD2_ShowLives(Lives);
        LCD2ScoreShadow = Score;
        LCD2LivesShadow = Lives;
    }
    if(levelChanged){
        LCD2_ShowLevel(lv);
        if(LCD2LevelShadow != 0xFF && lv > LCD2LevelShadow){
            // level-up notification on LCD1 event banner
            char buf[16];
            buf[0]='L'; buf[1]='V'; buf[2]='L'; buf[3]=' ';
            buf[4]=(char)('0'+lv); buf[5]='!'; buf[6]=' '; buf[7]='U';
            buf[8]='P'; buf[9]='!'; buf[10]='\0';
            LCD2_ShowEventMsg(buf, 0xFFE0u, 60);  // yellow flash
        }
        LCD2LevelShadow = lv;
    }
    LCD1_Select();
}
static void LCD2_ShowEventMsg(const char* msg, uint16_t color, uint8_t frames){
    if(!LCD2Enabled) return;
    LCD2_Sel();
    ST7735_FillRect(0, 50, 128, 12, 0x0000u);
    ST7735_SetCursor(1, 5);
    ST7735_SetTextColor(color);
    ST7735_OutString((char*)msg);
    LCD2MsgTimer = frames;
    LCD1_Select();
}
static void LCD2_TickEventMsg(void){
    if(LCD2MsgTimer == 0) return;
    LCD2MsgTimer--;
    if(LCD2MsgTimer == 0){
        LCD2_Sel();
        ST7735_FillRect(0, 50, 128, 12, 0x0000u);
        LCD1_Select();
    }
}

// ── Cursor helpers ─────────────────────────────────────────────────────────
static int16_t Cursor_MaxY(void){
    if(S == S_TITLE)    return 118;
    if(S == S_SETTINGS) return 84;
    if(S == S_LANGUAGE) return 72;
    return LCD1_HEIGHT - 1 - CURSOR_MAX_HALF_H;
}
static void Cursor_Reset(int16_t x, int16_t y){
    BladeCursor.x = x;
    BladeCursor.y = y;
    BladeCursor.radius = (uint8_t)CURSOR_MAX_HALF_W;
}
static uint8_t Cursor_IsSlicing(void){
    return (uint8_t)(!(Switch_In() & 0x01u));
}
static void Cursor_Sample(void){
    Joystick_Read(&JoyX, &JoyY);
    int16_t nx, ny;
    if(S == S_TITLE){
        nx = Clamp16(MapAxis(JoyX, LCD1_WIDTH-1), CURSOR_MAX_HALF_W, LCD1_WIDTH-1-CURSOR_MAX_HALF_W);
        ny = 114;
    } else if(S == S_SETTINGS || S == S_LANGUAGE){
        nx = 80;
        ny = (int16_t)(JoyY * 127 / 4095);
    } else {
        nx = Clamp16(MapAxis(JoyX, LCD1_WIDTH-1), CURSOR_MAX_HALF_W, LCD1_WIDTH-1-CURSOR_MAX_HALF_W);
        ny = Clamp16(MapAxis(JoyY, LCD1_HEIGHT-1), CURSOR_MAX_HALF_H, Cursor_MaxY());
    }
    if(Abs16(nx - BladeCursor.x) >= 2) BladeCursor.x = nx;
    if(Abs16(ny - BladeCursor.y) >= 2) BladeCursor.y = ny;
}
static uint8_t Cursor_IsHoveringSelectable(void){
    if(S == S_TITLE)    return (BladeCursor.y >= 108 && BladeCursor.y <= 122);
    if(S == S_SETTINGS) return (BladeCursor.y >= 24 && BladeCursor.y <= 80);
    if(S == S_LANGUAGE) return (BladeCursor.y >= 33 && BladeCursor.y <= 66);
    return 0;
}
static void Cursor_UpdateColor(void){
    uint8_t held = (uint8_t)(!(Switch_In() & 0x01u));
    if(held)                              CursorColor = ST7735_RED;
    else if(Cursor_IsHoveringSelectable()) CursorColor = ST7735_YELLOW;
    else                                  CursorColor = ST7735_CYAN;
}

static void Cursor_Draw(void){
    if(Cursor_IsSlicing()){
        DrawBitmapMasked(BladeCursor.x - (CURSOR2_W / 2),
                         BladeCursor.y + (CURSOR2_H / 2),
                         cursor2, CURSOR2_W, CURSOR2_H);
    } else {
        DrawBitmapMasked(BladeCursor.x - (CURSOR1_W / 2),
                         BladeCursor.y + (CURSOR1_H / 2),
                         cursor1, CURSOR1_W, CURSOR1_H);
    }
}
// ── Slash trail ────────────────────────────────────────────────────────────
static void Trail_Init(void){
}
static void Trail_Update(void){
}

// ── Gameplay border ────────────────────────────────────────────────────────
static void Gameplay_DrawBorder(void){
    LCD1_Select();
}

static uint8_t DifficultyLevel(void){
    // Level 0-9: one level per 10 points scored.
    uint8_t level = (uint8_t)(Score / 10U);
    return (level > 9U) ? 9U : level;
}
static uint8_t Fruit_SpawnChance(void){
    // Spawn probability per frame (%): 3 at level 0, up to 21 at level 9.
    return (uint8_t)(3U + DifficultyLevel() * 2U);
}
static uint8_t Fruit_ActiveCount(void){
    uint8_t count = 0;
    for(int i = 0; i < MAX_FRUITS; i++){
        if(Fruits[i].active) count++;
    }
    return count;
}
static uint8_t Fruit_SpawnIsClear(uint8_t type, int16_t x, int16_t y){
    int16_t left = x;
    int16_t right = x + P[type].xp;
    int16_t top = y - P[type].yp + 1;   
    int16_t bottom = y;
    for(int i = 0; i < MAX_FRUITS; i++){
        if(!Fruits[i].active) continue;
        int16_t pad = ((type == FRUIT_BOMB) || (Fruits[i].type == FRUIT_BOMB) ||
                       (type == FRUIT_BOOM) || (Fruits[i].type == FRUIT_BOOM)) ? 14 : 6;
        int16_t otherLeft = Fruits[i].x;
        int16_t otherRight = Fruits[i].x + P[Fruits[i].type].xp;
        int16_t otherTop = Fruits[i].y - P[Fruits[i].type].yp + 1;
        int16_t otherBottom = Fruits[i].y;
        if((right + pad) < otherLeft || (otherRight + pad) < left) continue;
        if((bottom + pad) < otherTop || (otherBottom + pad) < top) continue;
        return 0;
    }
    return 1;
}

// ── Fruit system ───────────────────────────────────────────────────────────
static void Fruit_Init(void){
    for(int i = 0; i < MAX_FRUITS; i++){
        Fruits[i].active = false; Fruits[i].sliced = false;
        Fruits[i].wasVisible = false;
        Fruits[i].hitCount = 0;   Fruits[i].deathTimer = 0;
        Fruits[i].slicedTimer = 0;
    }
    GravCounter = 0; SlowmoTimer = 0; SlowmoFrame = 0;
}

static void Fruit_Update(void){
    GravCounter++;
    if(SlowmoTimer > 0){
        SlowmoTimer--;
        SlowmoFrame++;
        if(SlowmoFrame % 3 != 0) return;  // only update physics every 3rd frame
    }
    for(int i = 0; i < MAX_FRUITS; i++){
        if(!Fruits[i].active){
            uint8_t level = DifficultyLevel();
            int16_t spawnY;
            int16_t spawnX;
            uint8_t t;
            uint8_t placed = 0;
            if(Fruit_ActiveCount() >= (uint8_t)(2U + (level / 2U) + 1U)) continue;
            if((rand() % 100) >= Fruit_SpawnChance()) continue;
            // Bomb probability scales from 1-in-20 at level 0 to 5-in-20 at level 9.
            uint8_t bombThresh = (uint8_t)(1U + level / 2U);
            int r = rand() % 20;
            if     (r < (int)bombThresh) t = FRUIT_BOMB;
            else if(r < (int)(bombThresh + 2)) t = FRUIT_POMEGRANATE;
            else           t = (uint8_t)((rand() % 3) * 2);  // 0, 2, or 4
            Fruits[i].type = t;
            Fruits[i].image = selection[t];
            int16_t sw = P[t].xp;
            spawnY = (int16_t)(LCD1_HEIGHT + P[t].yp);
            for(int tries = 0; tries < 10; tries++){
                spawnX = (int16_t)(3 + rand() % (LCD1_WIDTH - sw - 6));
                if(Fruit_SpawnIsClear(t, spawnX, spawnY)){
                    placed = 1;
                    break;
                }
            }
            if(!placed) continue;
            Fruits[i].x = spawnX;
            Fruits[i].y = spawnY;
            Fruits[i].vy = (int16_t)(-(7 + rand() % 4 + level));  // faster launch at higher levels
            Fruits[i].vx = (int16_t)((rand() % (5 + level)) - (2 + (level / 2)));
            Fruits[i].active = true; Fruits[i].sliced = false; Fruits[i].wasVisible = false;
            Fruits[i].hitCount = 0;  Fruits[i].deathTimer = 0; Fruits[i].slicedTimer = 0;
            continue;
        }
        // Death countdown (sliced fruit or bomb explosion)
        if(Fruits[i].deathTimer > 0){
            Fruits[i].deathTimer--;
            if(Fruits[i].deathTimer == 0){
                if(Fruits[i].type == FRUIT_BOOM && Lives > 0) Lives--;
                Fruits[i].active = false;
            }
            if(Fruits[i].type == FRUIT_BOOM) continue;  // exploding bomb: freeze
        }
        if(!Fruits[i].active) continue;

        // // Pomegranate re-slice timer
        // if(Fruits[i].slicedTimer > 0){
        //     Fruits[i].slicedTimer--;
        //     if(Fruits[i].slicedTimer == 0 && Fruits[i].type == FRUIT_POMEGRANATE + 1){
        //         Fruits[i].type   = FRUIT_POMEGRANATE;
        //         Fruits[i].image  = selection[FRUIT_POMEGRANATE];
        //         Fruits[i].sliced = false;
        //     }
        // }

        // Physics — gravity applies every 3rd frame at level 0, every 2nd at level 5+.
        uint8_t gravMod = (DifficultyLevel() >= 5u) ? 2u : 3u;
        if(GravCounter % gravMod == 0) Fruits[i].vy++;
        Fruits[i].x += Fruits[i].vx;
        Fruits[i].y += Fruits[i].vy;

        // Wall bounce
        if(Fruits[i].x < 2){ Fruits[i].x = 2; Fruits[i].vx = -Fruits[i].vx; }
        int16_t rx = (int16_t)(LCD1_WIDTH - 2 - P[Fruits[i].type].xp);
        if(Fruits[i].x > rx){ Fruits[i].x = rx; Fruits[i].vx = -Fruits[i].vx; }

        // Fruit has entered the visible gameplay field at least once.
        if((Fruits[i].y - P[Fruits[i].type].yp + 1) < LCD1_HEIGHT){
            Fruits[i].wasVisible = true;
        }

        // Off-bottom: miss penalty for untouched fruit
        if(Fruits[i].wasVisible &&
           Fruits[i].y > LCD1_HEIGHT + P[Fruits[i].type].yp){
            if(Fruits[i].hitCount == 0 &&
               Fruits[i].type != FRUIT_BOMB && Fruits[i].type != FRUIT_BOOM)
                if(Lives > 0) Lives--;
            Fruits[i].active = false;
        }
    }
}

// Scanline renderer: processes every screen row top-to-bottom.
// Each row is cleared to white then immediately painted with all sprite and
// cursor pixels that land on that row — so no row stays white for longer than
// one row-width of SPI transfer time.  This eliminates the full-frame white
// flash that a separate FillRect + DrawBitmap pass would produce.
static void DrawScanline(int16_t sy,
                         const uint16_t *img, int16_t x,
                         int16_t top, int16_t w, int16_t h){
    if(sy < top || sy > top + h - 1) return;
    int16_t imgRow = (h - 1) - (sy - top);
    const uint16_t *rp = &img[imgRow * w];
    int16_t col = 0;
    while(col < w){
        if(rp[col] == 0xFFFF){ col++; continue; }
        int16_t rs = col;
        while(col < w && rp[col] != 0xFFFF) col++;
        ST7735_DrawBitmap(x + rs, sy, rp + rs, col - rs, 1);
    }
}

static void DrawBitmapMasked(int16_t x, int16_t y,
                             const uint16_t *image, int16_t w, int16_t h){
    int16_t top = y - h + 1;
    for(int16_t sy = top; sy <= y; sy++){
        DrawScanline(sy, image, x, top, w, h);
    }
}

static void Fruits_Draw(void){
    LCD1_Select();

    // Precompute cursor geometry once
    const uint16_t *cimg;
    int16_t cw, ch, cx, ctop;
    if(Cursor_IsSlicing()){
        cimg = cursor2; cw = CURSOR2_W; ch = CURSOR2_H;
        cx   = BladeCursor.x - CURSOR2_W / 2;
        ctop = BladeCursor.y + CURSOR2_H / 2 - CURSOR2_H + 1;
    } else {
        cimg = cursor1; cw = CURSOR1_W; ch = CURSOR1_H;
        cx   = BladeCursor.x - CURSOR1_W / 2;
        ctop = BladeCursor.y + CURSOR1_H / 2 - CURSOR1_H + 1;
    }

    for(int16_t sy = 0; sy < LCD1_HEIGHT; sy++){
        // 1. Clear this row to white (fast: one SPI window + 160 identical pixels)
        ST7735_DrawFastHLine(0, sy, LCD1_WIDTH, ST7735_WHITE);
        // 2. Paint active fruit pixels on this row
        for(int i = 0; i < MAX_FRUITS; i++){
            if(!Fruits[i].active) continue;
            int16_t w = P[Fruits[i].type].xp;
            int16_t h = P[Fruits[i].type].yp;
            DrawScanline(sy, Fruits[i].image, Fruits[i].x,
                         Fruits[i].y - h + 1, w, h);
        }
        // 3. Paint cursor pixels on this row (always on top)
        DrawScanline(sy, cimg, cx, ctop, cw, ch);
    }
    Gameplay_DrawBorder();
}

static void Collision_Check(void){
    if(Switch_In() & 0x01u) return;  // button released (active-low) → not slicing
    int sliceCount = 0;
    for(int i = 0; i < MAX_FRUITS; i++){
        if(!Fruits[i].active || Fruits[i].sliced) continue;
        int16_t fw = P[Fruits[i].type].xp, fh = P[Fruits[i].type].yp;
        int16_t cx = BladeCursor.x, cy = BladeCursor.y;
        const int16_t M = 6;
        bool hit = (cx >= Fruits[i].x - M) && (cx <= Fruits[i].x + fw + M) &&
                   (cy >= Fruits[i].y - fh - M) && (cy <= Fruits[i].y + M);
        if(!hit) continue;
        Fruits[i].hitCount++;
        if(Fruits[i].type == FRUIT_BOMB){
            Sound_Explosion();
            Lives--;
            Fruits[i].type = FRUIT_BOOM; Fruits[i].image = selection[FRUIT_BOOM];
            Fruits[i].sliced = true; Fruits[i].deathTimer = EXPLODE_FRAMES;
            Fruits[i].vy = 0; Fruits[i].vx = 0;
            LCD2_ShowEventMsg(lang("HISS! -1 LIFE","SSSS! -1 VIDA"), 0x001Fu, 60);
        } else {
            Fruits[i].type = Fruits[i].type + 1;   // whole → cut
            Fruits[i].image = selection[Fruits[i].type];
            Fruits[i].sliced = true;
            Fruits[i].deathTimer = 0;
            Fruits[i].vy = (int16_t)(2 + (Abs16(Fruits[i].vy) / 3));
            Sound_Slice();
            sliceCount++;
        }
    }
    if(sliceCount > 0){
        Score += (uint32_t)(sliceCount * sliceCount);  // N² scoring
        if(sliceCount == 2) LCD2_ShowEventMsg(lang("PURR-FECT x2!","PERFECTO x2! "), 0x07E0u, 55);
        else if(sliceCount >= 3) LCD2_ShowEventMsg(lang("PURR-FECT MEGA!","PERFECTO MEGA!"), 0x07FFu, 55);
    }
}

static void Title_DrawLogo(uint8_t frame){
    int16_t w = TitleLogoW[frame], h = TitleLogoH[frame];
    int16_t x = (int16_t)((LCD1_WIDTH - w) / 2);
    LCD1_Select();
    ST7735_FillRect(0, TITLE_TEXT_Y + 1, LCD1_WIDTH,
                    TITLE_SPRITE_Y - TITLE_TEXT_Y + 1, ST7735_BLACK);
    ST7735_DrawBitmap(x, TITLE_SPRITE_Y, TitleLogos[frame], w, h);
}
static void Title_AnimateLogo(void){
    titleLogoTimer++;
    if(titleLogoTimer < 8) return;
    titleLogoTimer = 0;
    titleLogoFrame = 0;
    Title_DrawLogo(titleLogoFrame);
}

// ─────────────────────────────────────────────────────────────────────────
// TITLE SCREEN
// ─────────────────────────────────────────────────────────────────────────
void Title_Init(void){
    LED_SetLives(0);
    LCD1_Select();
    ST7735_FillScreen(ST7735_BLACK);
    prevSelection = MENU_NONE; titleCursor = MENU_NONE;
    titleLogoFrame = 0; titleLogoTimer = 0;
    Cursor_Reset(80, 114);
    
    Title_DrawLogo(titleLogoFrame);
    Title_RenderButton(0, 0); Title_RenderButton(1, 0); Title_RenderButton(2, 0);
}

static void Title_RenderButton(uint8_t slot, uint8_t selected){
    static const uint8_t cols[3] = {2, 8, 20};
    const char *labels[3] = {
        lang("INFO",     "INFO"),
        lang("SETTINGS", "AJUSTES "),
        lang("PLAY",     "JUGAR")
    };
    LCD1_Select();
    ST7735_SetCursor(cols[slot], TITLE_BUTTON_ROW);
    ST7735_SetTextColor(selected ? ST7735_YELLOW : ST7735_WHITE);
    ST7735_OutString((char*)labels[slot]);
}

void Title_HighlightMenu(void){
    uint8_t sel = (BladeCursor.x < 53) ? 0u : (BladeCursor.x < 107) ? 1u : 2u;
    titleCursor = sel;
    if(sel == prevSelection) return;
    LCD1_Select();
    for(int i = 0; i < 3; i++) Title_RenderButton((uint8_t)i, i == (int)sel);
    prevSelection = sel;
}

// ─────────────────────────────────────────────────────────────────────────
// INSTRUCTIONS SCREEN
// ─────────────────────────────────────────────────────────────────────────
void Instructions_Init(void){
    LCD1_Select(); ST7735_FillScreen(ST7735_BLACK);
    ST7735_SetTextColor(ST7735_YELLOW);
    ST7735_SetCursor(3, 0); ST7735_OutString((char*)lang("HOW 2 PURR-LAY", "COMO JUGAR"));
    DrawSep(13, ST7735_YELLOW);

    ST7735_SetTextColor(ST7735_WHITE);
    ST7735_SetCursor(0, 2); ST7735_OutString((char*)lang("> Slash fruits", "> Corta frutas"));
    ST7735_SetCursor(2, 3); ST7735_OutString((char*)lang("for points!", "para puntos!"));
    ST7735_SetTextColor(ST7735_RED);
    ST7735_SetCursor(0, 4); ST7735_OutString((char*)lang("> Dodge the bombs", "> Evita bombas!"));
    ST7735_SetTextColor(ST7735_WHITE);
    ST7735_SetCursor(2, 5); ST7735_OutString((char*)lang("bomb = -1 life", "bomba=-1 vida"));
    ST7735_SetCursor(0, 6); ST7735_OutString((char*)lang("> 3 lives total", "> 3 vidas total"));
    ST7735_SetCursor(2, 7); ST7735_OutString((char*)lang("3 misses = KO!", "3 fallas = fin"));
    ST7735_SetTextColor(0xFD20u);
    ST7735_SetCursor(0, 9); ST7735_OutString((char*)lang("> Slice every fruit", "> Corta toda fruta"));
    ST7735_SetCursor(2,10); ST7735_OutString((char*)lang("before it drops!", "antes de caer!"));
    ST7735_SetTextColor(ST7735_CYAN);
    ST7735_SetCursor(1,12); ST7735_OutString((char*)lang("Bottom btn: back", "Abajo: volver"));
}

// ─────────────────────────────────────────────────────────────────────────
// SETTINGS SCREEN
// ─────────────────────────────────────────────────────────────────────────
void Settings_Init(void){
    LCD1_Select(); ST7735_FillScreen(ST7735_BLACK);
    settingsSelection = MENU_NONE; settingsCursor = MENU_NONE;
    ST7735_SetTextColor(ST7735_YELLOW);
    ST7735_SetCursor((CurrentLanguage == LANG_SPANISH) ? 10 : 9, 0);
    ST7735_OutString((char*)lang("SETTINGS", "AJUSTES"));
    DrawSep(13, ST7735_YELLOW);
    Settings_RenderOption(0, lang("Language", "Idioma  "), 0);
    Settings_RenderOption(1, lang("Credits ", "Creditos"), 0);
    Settings_RenderOption(2, lang("<- Back ","Volver  "), 0);
    DrawSep(105, ST7735_YELLOW);
    ST7735_SetTextColor(ST7735_CYAN);
    ST7735_SetCursor(0,11); ST7735_OutString((char*)lang("Joy: move cursor", "Mueve cursor"));
    ST7735_SetCursor(0,12); ST7735_OutString((char*)lang("Top:sel  Bot:back", "Arriba: sel"));
}
static void Settings_RenderOption(uint8_t row, const char* label, uint8_t selected){
    static const uint8_t rows[3] = {3, 5, 7};
    LCD1_Select();
    ST7735_SetCursor(2, rows[row]);
    ST7735_SetTextColor(selected ? ST7735_YELLOW : ST7735_WHITE);
    ST7735_OutChar(selected ? '>' : ' '); ST7735_OutChar(selected ? '>' : ' ');
    ST7735_OutString((char*)label);
}
void Settings_Highlight(void){
    uint8_t next = (BladeCursor.y < 43) ? 0u : (BladeCursor.y < 86) ? 1u : 2u;
    settingsCursor = next;
    if(settingsCursor == settingsSelection) return;
    const char* labels[3] = {
        lang("Language","Idioma  "), lang("Credits ","Creditos"), lang("<- Back ","Volver  ")
    };
    LCD1_Select();
    DrawSep(13, ST7735_YELLOW);
    ST7735_SetTextColor(ST7735_YELLOW);
    ST7735_SetCursor((CurrentLanguage==LANG_SPANISH)?10:9, 0);
    ST7735_OutString((char*)lang("SETTINGS","AJUSTES"));
    for(int i = 0; i < 3; i++) Settings_RenderOption((uint8_t)i, labels[i], i==(int)settingsCursor);
    settingsSelection = settingsCursor;
}

// ─────────────────────────────────────────────────────────────────────────
// LANGUAGE SCREEN
// ─────────────────────────────────────────────────────────────────────────
void Language_Init(void){
    LCD1_Select(); ST7735_FillScreen(ST7735_BLACK);
    languageSelection = MENU_NONE; languageCursor = MENU_NONE;
    Cursor_Reset(80, 52);
    ST7735_SetTextColor(ST7735_YELLOW);
    ST7735_SetCursor((CurrentLanguage==LANG_SPANISH)?10:9, 0); ST7735_OutString((char*)lang("LANGUAGE","IDIOMA"));
    DrawSep(13, ST7735_YELLOW);
    Language_RenderOption(LANG_ENGLISH, "ENGLISH", 0);
    Language_RenderOption(LANG_SPANISH, "ESPANOL", 0);
    ST7735_SetCursor(2, 8); ST7735_SetTextColor(ST7735_DARKGREY);
    ST7735_OutString((char*)lang("Active: ","Activo: "));
    ST7735_SetTextColor(ST7735_GREEN);
    ST7735_OutString((char*)(CurrentLanguage==LANG_ENGLISH ? "English" : "Espanol"));
    DrawSep(105, ST7735_YELLOW);
    ST7735_SetTextColor(ST7735_CYAN);
    ST7735_SetCursor(0,11); ST7735_OutString((char*)lang("Joy:pick  Top:save","Mueve a idioma"));
    ST7735_SetCursor(0,12); ST7735_OutString((char*)lang("Bot: back","Abajo: volver"));
}
static void Language_RenderOption(uint8_t row, const char* label, uint8_t selected){
    static const uint8_t rows[2] = {4, 6};
    LCD1_Select();
    ST7735_SetCursor(2, rows[row]);
    ST7735_SetTextColor(selected ? ST7735_YELLOW : ST7735_WHITE);
    ST7735_OutString(selected ? (char*)">>" : (char*)"  ");
    ST7735_OutString((char*)label);
}
void Language_Highlight(void){
    uint8_t next = (BladeCursor.y < 64) ? LANG_ENGLISH : LANG_SPANISH;
    languageCursor = next;
    if(languageCursor == languageSelection) return;
    static const char* labels[2] = {"ENGLISH","ESPANOL"};
    LCD1_Select();
    DrawSep(13, ST7735_YELLOW);
    ST7735_SetTextColor(ST7735_YELLOW);
    ST7735_SetCursor((CurrentLanguage==LANG_SPANISH)?10:9, 0); ST7735_OutString((char*)lang("LANGUAGE","IDIOMA"));
    for(int i = 0; i < 2; i++) Language_RenderOption((uint8_t)i, labels[i], i==(int)languageCursor);
    ST7735_SetCursor(2, 8); ST7735_SetTextColor(ST7735_DARKGREY);
    ST7735_OutString((char*)lang("Active: ","Activo: "));
    ST7735_SetTextColor(ST7735_GREEN);
    ST7735_OutString((char*)(CurrentLanguage==LANG_ENGLISH ? "English" : "Espanol"));
    ST7735_OutString((char*)"  ");
    languageSelection = languageCursor;
}

// ─────────────────────────────────────────────────────────────────────────
// CREDITS SCREEN
// ─────────────────────────────────────────────────────────────────────────
void Credits_Init(void){
    LCD1_Select(); ST7735_FillScreen(ST7735_BLACK);
    ST7735_SetTextColor(ST7735_YELLOW);
    ST7735_SetCursor(7, 0); ST7735_OutString((char*)lang("COOL CATS","GATOS GENIALES"));
    DrawSep(13, ST7735_YELLOW);
    ST7735_SetTextColor(ST7735_WHITE);
    ST7735_SetCursor(4, 3); ST7735_OutString((char*)"Kaitlyn C.");
    ST7735_SetCursor(4, 4); ST7735_OutString((char*)"Keerthana M.");
    ST7735_SetTextColor(ST7735_CYAN);
    ST7735_SetCursor(4, 6); ST7735_OutString((char*)"ECE319H  Lab 9");
    ST7735_SetCursor(6, 7); ST7735_OutString((char*)"Spring 2026");
    ST7735_SetTextColor(ST7735_MAGENTA);
    ST7735_SetCursor(4, 9); ST7735_OutString((char*)lang("Meow! Meow!","Miau! Miau!"));
    ST7735_SetCursor(7,10); ST7735_OutString((char*)lang("Meoooow!","Miaaaaaau!"));
    DrawSep(115, ST7735_YELLOW);
    ST7735_SetTextColor(ST7735_CYAN);
    ST7735_SetCursor(2,12); ST7735_OutString((char*)lang("Bottom btn: back","Boton abajo"));
}

// ─────────────────────────────────────────────────────────────────────────
// GAMEPLAY SCREEN
// ─────────────────────────────────────────────────────────────────────────
void Gameplay_Init(void){
    Sound_Stop();
    LCD1_Select(); ST7735_FillScreen(ST7735_WHITE);
    // Only reset score/lives on a fresh start; preserve them when resuming from pause.
    if(PreviousState != (uint32_t)S_PAUSED){
        Score = 0; Lives = 3;
        LED_SetLives(3);
    }
    Cursor_Reset(80, 64);
    Trail_Init();
    Fruit_Init();
    Gameplay_DrawBorder();
    LCD2ScoreShadow = 0xFFFFFFFFu; LCD2LivesShadow = 0xFF;
    LCD2LevelShadow = 0xFF; LCD2MsgTimer = 0;
    LCD2_UpdateGameplayStatus();
}

// ─────────────────────────────────────────────────────────────────────────
// PAUSE SCREEN (overlay on top of gameplay field)
// ─────────────────────────────────────────────────────────────────────────
void Pause_Init(void){
    LCD1_Select();
    ST7735_FillRect(20, 42, 120, 44, ST7735_BLACK);
    ST7735_SetTextColor(ST7735_WHITE);
    ST7735_SetCursor(5, 5); ST7735_OutString((char*)lang("-- PAWS-ED --","-- PAUSA --"));
    ST7735_SetTextColor(ST7735_CYAN);
    ST7735_SetCursor(3, 7); ST7735_OutString((char*)lang("Bottom: resume","Abajo: seguir"));
    ST7735_SetCursor(4, 8); ST7735_OutString((char*)lang("Top: title","Arriba: inicio"));
}

// ─────────────────────────────────────────────────────────────────────────
// GAME OVER SCREEN
// ─────────────────────────────────────────────────────────────────────────
void GameOver_Init(void){
    Sound_Sad();
    LCD1_Select(); ST7735_FillScreen(ST7735_BLACK);
    LED_SetLives(0);
    ST7735_SetTextColor(ST7735_RED);
    ST7735_SetCursor(4, 2); ST7735_OutString((char*)lang("CAT-ASTROPHE!","GATO-STROFE!"));
    ST7735_SetTextColor(ST7735_WHITE);
    ST7735_SetCursor(4, 5); ST7735_OutString((char*)lang("Score: ","Puntos: "));
    ST7735_SetTextColor(ST7735_YELLOW); ST7735_OutUDec(Score);
    if(Score > HighScore){
        HighScore = Score;
        ST7735_SetTextColor(ST7735_GREEN);
        ST7735_SetCursor(2, 7); ST7735_OutString((char*)lang("IM-PAWS-IBLE!","INCREIBLE!   "));
    }
    ST7735_SetTextColor(ST7735_DARKGREY);
    ST7735_SetCursor(3, 9); ST7735_OutString((char*)lang("Best: ","Mejor: ")); ST7735_OutUDec(HighScore);
    ST7735_SetTextColor(ST7735_CYAN);
    ST7735_SetCursor(1,11); ST7735_OutString((char*)lang("Top: try again","Arriba: reiniciar"));
    LCD2ScoreShadow = 0xFFFFFFFFu; LCD2LivesShadow = 0xFF;
    LCD2LevelShadow = 0xFF;
    LCD2_UpdateGameplayStatus();
}

// ─────────────────────────────────────────────────────────────────────────
// FSM TABLE
// ─────────────────────────────────────────────────────────────────────────
State_t FSM[8] = {
    { &Title_Init,        { S_TITLE,        S_INSTRUCTIONS, S_SETTINGS, S_GAMEPLAY }},
    { &Instructions_Init, { S_INSTRUCTIONS, S_TITLE,        S_TITLE,    S_TITLE    }},
    { &Settings_Init,     { S_SETTINGS,     S_LANGUAGE,     S_CREDITS,  S_TITLE    }},
    { &Gameplay_Init,     { S_GAMEPLAY,     S_PAUSED,       S_GAMEOVER, S_GAMEPLAY }},
    { &Pause_Init,        { S_PAUSED,       S_GAMEPLAY,     S_GAMEPLAY, S_TITLE    }},
    { &GameOver_Init,     { S_GAMEOVER,     S_TITLE,        S_TITLE,    S_TITLE    }},
    { &Language_Init,     { S_LANGUAGE,     S_SETTINGS,     S_SETTINGS, S_SETTINGS }},
    { &Credits_Init,      { S_CREDITS,      S_SETTINGS,     S_SETTINGS, S_SETTINGS }},
};

// ─────────────────────────────────────────────────────────────────────────
// INPUT FUNCTIONS
// ─────────────────────────────────────────────────────────────────────────
static uint32_t Title_Input(void){
    if(titleCursor != MENU_NONE && Switch_Pressed(TOP_BUTTON)) return titleCursor + 1;
    return 0;
}
static uint32_t Back_Input(void){
    if(Switch_Pressed(BOTTOM_BUTTON) || Switch_Pressed(TOP_BUTTON)) return 1;
    return 0;
}
static uint32_t Settings_Input(void){
    if(Switch_Pressed(BOTTOM_BUTTON)) return 3;
    if(settingsCursor != MENU_NONE && Switch_Pressed(TOP_BUTTON)) return settingsCursor + 1;
    return 0;
}
static uint32_t Language_Input(void){
    if(Switch_Pressed(BOTTOM_BUTTON)) return 1;
    if(languageCursor != MENU_NONE && Switch_Pressed(TOP_BUTTON)){
        CurrentLanguage = languageCursor; return 1;
    }
    return 0;
}
static uint32_t Credits_Input(void){
    if(Switch_Pressed(BOTTOM_BUTTON) || Switch_Pressed(TOP_BUTTON)) return 1;
    return 0;
}
static uint32_t Gameplay_Input(void){
    if(Lives == 0) return 2;
    if(Switch_Pressed(BOTTOM_BUTTON)) return 1;
    return 0;
}
static uint32_t Pause_Input(void){
    if(Switch_Pressed(BOTTOM_BUTTON)) return 1;
    if(Switch_Pressed(TOP_BUTTON))    return 3;
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────
// PUBLIC API
// ─────────────────────────────────────────────────────────────────────────
void Game_Init(void){
    Joystick_Init();
    S = S_TITLE; PreviousState = 0xFF; Semaphore = 0;
    Score = 0; Lives = 3; HighScore = 0;
    Cursor_Reset(80, 104);
}

uint32_t Game_GetInput(void){
    if     (S == S_TITLE)        return Title_Input();
    else if(S == S_INSTRUCTIONS) return Back_Input();
    else if(S == S_SETTINGS)     return Settings_Input();
    else if(S == S_GAMEPLAY)     return Gameplay_Input();
    else if(S == S_PAUSED)       return Pause_Input();
    else if(S == S_GAMEOVER)     return Back_Input();
    else if(S == S_LANGUAGE)     return Language_Input();
    else if(S == S_CREDITS)      return Credits_Input();
    return 0;
}

void Game_UpdateFrame(void){
    Cursor_Sample();

    if(S != PreviousState){
        (FSM[S].InitPt)();
        {
            const unsigned short *img = excited_cat; int16_t w=89, h=65;
            if(S == S_PAUSED)       { img=sleeping_cat; w=100; }
            else if(S == S_GAMEOVER){ img=sleeping_cat;  w=100; }
            LCD2_ShowReaction(img, w, h);
        }
        LCD1_Select();
        PreviousState = S;
    }

    Cursor_UpdateColor();

    if(S == S_TITLE){
        Title_AnimateLogo();
        Title_HighlightMenu();
    } else if(S == S_SETTINGS){
        Settings_Highlight();
    } else if(S == S_LANGUAGE){
        Language_Highlight();
    } else if(S == S_GAMEPLAY){
        Fruit_Update();
        Collision_Check();
        Fruits_Draw();      // scanline: clear+fruits+cursor in one top-to-bottom pass
        Trail_Update();
        LED_SetLives(Lives);
        LCD2_UpdateGameplayStatus();
        LCD2_TickEventMsg();
    }
}
