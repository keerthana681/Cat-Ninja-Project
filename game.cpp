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
// selection[] pairs whole/cut sprites at even/odd indices.
// When a fruit is sliced its type index is incremented by 1, switching
// from the whole sprite to the cut sprite with no extra logic.
static const unsigned short* selection[10] = {
    wholebluefish,  cutbluefish,
    wholeorangefish,  cutorangefish,
    wholejellyfish,   cutjellyfish,
    wholegreenfish, cutgreenfish,
    bomb,             boom
};
// P[type] stores the pixel width (xp) and height (yp) of each sprite.
// ST7735_DrawBitmap anchors y at the bottom of the image, so the top
// pixel row sits at (y - yp + 1).  These dimensions must match the
// actual array sizes declared in images.h.
typedef struct { int16_t xp, yp; } FruitDim;
static const FruitDim P[10] = {
    {48,14},{48,15},   // 0-1  bluefish
    {36,30},{44,30},   // 2-3  watermelon
    {20,35},{21,36},   // 4-5  pineapple
    {33,30},{39,30},   // 6-7  pomegranate
    {32,48},{69,59}    // 8-9  bomb/boom
};
static const unsigned short* TitleLogos[1] = {titlecat4};
static const int16_t TitleLogoW[1] = { 105 };
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
    int16_t  x, y;          // pixel position; y is the bottom anchor for DrawBitmap
    int16_t  vx, vy;        // velocity in px/frame; vy positive = moving downward
    uint8_t  type;          // index into selection[] and P[] (even=whole, odd=cut)
    bool     active;        // false = slot is free and can be reused
    bool     sliced;        // true once the player has hit this fruit
    bool     wasVisible;    // set when the sprite's top edge enters the screen;
                            // prevents penalizing a miss if the fruit never appeared
    uint8_t  hitCount;      // incremented each time the cursor overlaps this fruit
    uint8_t  deathTimer;    // counts down to 0, then sets active=false; used for
                            // sliced fruits that linger and for the bomb explosion
    const unsigned short* image; // pointer to the current sprite array (whole or cut)
} Fruit_t;

static Fruit_t Fruits[MAX_FRUITS];

// ── Game-wide state ────────────────────────────────────────────────────────
uint32_t          S;               // current FSM state index
volatile uint32_t Semaphore;       // set to 1 by TIMG12 ISR at 30 Hz; main loop clears it
uint8_t           CurrentLanguage = LANG_ENGLISH;

static uint32_t PreviousState   = 0xFF;   // detects state transitions in Game_UpdateFrame
static uint32_t Score           = 0;
static uint32_t HighScore       = 0;
static uint8_t  Lives           = 3;
static const uint8_t MENU_NONE  = 0xFF;   // sentinel: no menu item currently highlighted

// LCD2 shadow values — track what was last drawn on LCD2 so we only
// re-send over SPI when a value actually changes (SPI writes are slow).
static uint32_t LCD2ScoreShadow = 0xFFFFFFFF;  // 0xFF… forces a full redraw on first frame
static uint8_t  LCD2LivesShadow = 0xFF;
static uint8_t  LCD2LevelShadow = 0xFF;
static const uint8_t LCD2Enabled = 1;
static uint8_t  LCD2MsgTimer   = 0;  // counts down frames until the event banner clears

// Menu highlight shadows — track the previously rendered selection so
// only the changed button/option is redrawn (avoids full-screen flicker).
static uint8_t prevSelection    = 0xFF;
static uint8_t settingsSelection = 0xFF;
static uint8_t languageSelection = 0xFF;
static uint8_t titleLogoFrame   = 0;
static uint8_t titleLogoTimer   = 0;
static uint8_t titleCursor      = MENU_NONE;
static uint8_t settingsCursor   = MENU_NONE;
static uint8_t languageCursor   = MENU_NONE;

// Joystick raw ADC readings (0-4095); initialized to mid-scale so the
// cursor starts centered before the first Joystick_Read() call.
static uint32_t JoyX = 2048, JoyY = 2048;
static uint16_t SlowmoTimer = 0;  // remaining frames of slow-motion effect
static uint8_t  SlowmoFrame = 0;  // frame counter within slow-mo (skips physics every 2/3)
static uint8_t  GravCounter = 0;  // increments every frame; gravity applies on modulo
static uint16_t GameFrames  = 0;  // frames elapsed this gameplay session (used by DifficultyLevel)

// Display constants
#define LCD1_WIDTH   160
#define LCD1_HEIGHT  128
#define LIGHT_GREY   0xC618u   // RGB565 (192,192,192)
// Dojo wood-plank floor palette (RGB565, LCD1)
#define WOOD_JOINT   0x118Au   // dark warm brown joint   (BGR565)
#define WOOD_LIGHT   0x54B9u   // honey/maple plank       (BGR565)
#define WOOD_DARK    0x3334u   // medium brown plank      (BGR565)
// Title screen layout (landscape 160×128):
//   y= 0-25 : "Cat Ninja" title text (catninjatitle 134×25, bottom anchor y=25)
//   y=26-92 : titlecat sprite (bottom anchor TITLE_SPRITE_Y=92, max h=64)
//   y=110+  : INFO / SETTINGS / PLAY buttons (row 11)
#define TITLE_TEXT_Y     25   // bottom anchor of catninjatitle
#define TITLE_SPRITE_Y   92   // bottom anchor of titlecat animation
#define TITLE_BUTTON_ROW 11   // ST7735 text row for menu buttons

//cursor stuff
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

static void Title_RenderButton(uint8_t slot, uint8_t selected);
static void Settings_RenderOption(uint8_t row, const char* label, uint8_t selected);
static void Language_RenderOption(uint8_t row, const char* label, uint8_t selected);
static void LCD2_UpdateGameplayStatus(void);
static void LCD2_ShowEventMsg(const char* msg, uint16_t color, uint8_t frames);
static void Fruits_Draw(void);
static void Fruit_Init(void);
static void Fruit_Update(void);
static void Collision_Check(void);
static void Title_DrawLogo(uint8_t frame);
static void Title_AnimateLogo(void);
static void DrawBitmapMasked(int16_t x, int16_t y, const uint16_t *image, int16_t w, int16_t h);
static uint8_t DifficultyLevel(void);

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

// lcd2 stuff
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

// cursor help
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
// Switch_In() bit 0 = TOP button, 1 when held.  Slicing is active when
// the button is NOT held (logical NOT), matching the hardware's active-low
// inversion already applied inside Switch_In().
static uint8_t Cursor_IsSlicing(void){
    return (uint8_t)(!(Switch_In() & 0x01u));
}
static void Cursor_Sample(void){
    Joystick_Read(&JoyX, &JoyY);
    int16_t nx, ny;
    if(S == S_TITLE){
        // Title: cursor locked to the button row (y fixed); only X moves.
        nx = Clamp16(MapAxis(JoyX, LCD1_WIDTH-1), CURSOR_MAX_HALF_W, LCD1_WIDTH-1-CURSOR_MAX_HALF_W);
        ny = 114;
    } else if(S == S_SETTINGS || S == S_LANGUAGE){
        // Menu screens: cursor locked to center column; Y selects the option row.
        nx = 80;
        ny = (int16_t)(JoyY * 127 / 4095);
    } else {
        // Gameplay: full 2-D cursor clamped to keep the sprite on-screen.
        nx = Clamp16(MapAxis(JoyX, LCD1_WIDTH-1), CURSOR_MAX_HALF_W, LCD1_WIDTH-1-CURSOR_MAX_HALF_W);
        ny = Clamp16(MapAxis(JoyY, LCD1_HEIGHT-1), CURSOR_MAX_HALF_H, Cursor_MaxY());
    }
    // 2-pixel deadband suppresses jitter from ADC noise near the resting position.
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

static uint8_t DifficultyLevel(void){
    // Level 0-9: rises by score (1 per 10 pts) or time (1 per 15 s), whichever is higher.
    uint8_t fromScore = (uint8_t)(Score / 10U);
    uint8_t fromTime  = (uint8_t)(GameFrames / 450U);  // 450 frames ≈ 15 s at 30 Hz
    uint8_t level = fromScore > fromTime ? fromScore : fromTime;
    return (level > 9U) ? 9U : level;
}
// Each frame, a new fruit is spawned with this percentage probability.
// Ranges from 3% at level 0 to 21% at level 9 (linear ramp).
static uint8_t Fruit_SpawnChance(void){
    return (uint8_t)(3U + DifficultyLevel() * 2U);
}
static uint8_t Fruit_ActiveCount(void){
    uint8_t count = 0;
    for(int i = 0; i < MAX_FRUITS; i++){
        if(Fruits[i].active) count++;
    }
    return count;
}
// Returns 1 if the candidate spawn position does not overlap any active fruit.
// Bombs and boom sprites use a larger padding (14 px) because their explosion
// sprites are bigger and visual overlap looks bad; regular fruit uses 6 px.
static uint8_t Fruit_SpawnIsClear(uint8_t type, int16_t x, int16_t y){
    int16_t left = x;
    int16_t right = x + P[type].xp;
    int16_t top = y - P[type].yp + 1;
    int16_t bottom = y;
    for(int i = 0; i < MAX_FRUITS; i++){
        if(!Fruits[i].active) continue;
        int16_t pad = ((type == FRUIT_BOMB) || (Fruits[i].type == FRUIT_BOMB) ||
                       (type == FRUIT_BOOM) || (Fruits[i].type == FRUIT_BOOM)) ? 14 : 6;
        int16_t otherLeft  = Fruits[i].x;
        int16_t otherRight = Fruits[i].x + P[Fruits[i].type].xp;
        int16_t otherTop   = Fruits[i].y - P[Fruits[i].type].yp + 1;
        int16_t otherBottom = Fruits[i].y;
        if((right + pad) < otherLeft || (otherRight + pad) < left) continue;
        if((bottom + pad) < otherTop  || (otherBottom + pad) < top) continue;
        return 0;  // overlaps an existing fruit
    }
    return 1;
}

// ── Fruit system ───────────────────────────────────────────────────────────
static void Fruit_Init(void){
    for(int i = 0; i < MAX_FRUITS; i++){
        Fruits[i].active = false; Fruits[i].sliced = false;
        Fruits[i].wasVisible = false;
        Fruits[i].hitCount = 0;   Fruits[i].deathTimer = 0;
    }
    GravCounter = 0; SlowmoTimer = 0; SlowmoFrame = 0; GameFrames = 0;
}

static void Fruit_Update(void){
    // Cap GameFrames so the difficulty ceiling (level 9) is reached at most
    // after 4500 / 30 = 150 seconds and the counter never wraps.
    if(GameFrames < 4500U) GameFrames++;
    GravCounter++;   // global tick used as the gravity modulus clock

    // Slow-motion: skip physics on 2 out of every 3 frames, making fruits
    // appear to float.  SlowmoFrame tracks position within the 3-frame cycle.
    if(SlowmoTimer > 0){
        SlowmoTimer--;
        SlowmoFrame++;
        if(SlowmoFrame % 3 != 0) return;
    }

    for(int i = 0; i < MAX_FRUITS; i++){
        if(!Fruits[i].active){
            // ── Spawn attempt for this empty slot ────────────────────────
            uint8_t level = DifficultyLevel();

            // Cap on-screen fruit count: starts at 3, adds one every 2 levels.
            if(Fruit_ActiveCount() >= (uint8_t)(2U + (level / 2U) + 1U)) continue;

            // Probabilistic gate: most frames produce no new fruit.
            if((rand() % 100) >= Fruit_SpawnChance()) continue;

            // Choose type: bombs scale from 3-in-20 at level 0 to 7-in-20 at level 9;
            // pomegranates are always 2-in-20; remaining slots are regular fruit.
            uint8_t bombThresh = (uint8_t)(3U + level / 2U);
            int r = rand() % 20;
            uint8_t t;
            if     (r < (int)bombThresh)        t = FRUIT_BOMB;
            else if(r < (int)(bombThresh + 2))  t = FRUIT_POMEGRANATE;  // pomo = 2 slots
            else                                t = (uint8_t)((rand() % 3) * 2);  // 0, 2, or 4

            Fruits[i].type  = t;
            Fruits[i].image = selection[t];
            int16_t sw = P[t].xp;

            // Spawn just below the bottom edge so the fruit flies upward into view.
            int16_t spawnY = (int16_t)(LCD1_HEIGHT + P[t].yp);
            int16_t spawnX = 0;
            uint8_t placed = 0;
            for(int tries = 0; tries < 10; tries++){
                spawnX = (int16_t)(3 + rand() % (LCD1_WIDTH - sw - 6));
                if(Fruit_SpawnIsClear(t, spawnX, spawnY)){ placed = 1; break; }
            }
            if(!placed) continue;  // no clear spot found this frame; try again next frame

            Fruits[i].x  = spawnX;
            Fruits[i].y  = spawnY;
            // Negative vy = upward velocity; magnitude grows with level for faster arcs.
            Fruits[i].vy = (int16_t)(-(7 + rand() % 4 + level));
            // Random lateral drift; range widens and bias shifts with level.
            Fruits[i].vx = (int16_t)((rand() % (5 + level)) - (2 + (level / 2)));
            Fruits[i].active  = true;  
            Fruits[i].sliced    = false;
            Fruits[i].wasVisible = false;
            Fruits[i].hitCount = 0;    
            Fruits[i].deathTimer = 0;
            continue;
        }

        // ── Death countdown (sliced fruit lingering or bomb explosion) ────
        if(Fruits[i].deathTimer > 0){
            Fruits[i].deathTimer--;
            if(Fruits[i].deathTimer == 0){
                // Boom sprite's timer expiring is when the life penalty fires,
                // giving the player a moment to see the explosion first.
                if(Fruits[i].type == FRUIT_BOOM && Lives > 0) Lives--;
                Fruits[i].active = false;
            }
            if(Fruits[i].type == FRUIT_BOOM) continue;  // freeze position during explosion
        }
        if(!Fruits[i].active) continue;

        // ── Physics ───────────────────────────────────────────────────────
        // Apply +1 vy (downward acceleration) at a rate that tightens with difficulty:
        // every 4th, 3rd, 2nd, or every frame for levels 0-2, 3-5, 6-7, 8-9.
        uint8_t lv = DifficultyLevel();
        uint8_t gravMod = (lv < 3u) ? 4u : (lv < 6u) ? 3u : (lv < 8u) ? 2u : 1u;
        if(GravCounter % gravMod == 0) Fruits[i].vy++;
        Fruits[i].x += Fruits[i].vx;
        Fruits[i].y += Fruits[i].vy;

        // Reflect off left/right walls so fruits don't clip off the edges.
        if(Fruits[i].x < 2){ Fruits[i].x = 2; Fruits[i].vx = -Fruits[i].vx; }
        int16_t rx = (int16_t)(LCD1_WIDTH - 2 - P[Fruits[i].type].xp);
        if(Fruits[i].x > rx){ Fruits[i].x = rx; Fruits[i].vx = -Fruits[i].vx; }

        // Mark as visible once the sprite's top edge enters the screen area.
        // wasVisible prevents penalizing a fruit that was spawned off-screen
        // and fell back down before the player ever saw it.
        if((Fruits[i].y - P[Fruits[i].type].yp + 1) < LCD1_HEIGHT){
            Fruits[i].wasVisible = true;
        }

        // Miss detection: fruit fell below the bottom of the screen without being sliced.
        // Bombs and booms are excluded — missing a bomb is not penalized.
        if(Fruits[i].wasVisible &&
           Fruits[i].y > LCD1_HEIGHT + P[Fruits[i].type].yp){
            if(Fruits[i].hitCount == 0 &&
               Fruits[i].type != FRUIT_BOMB && Fruits[i].type != FRUIT_BOOM)
                if(Lives > 0) Lives--;
            Fruits[i].active = false;
        }
    }
}

// DrawScanline renders one horizontal slice (row sy) of a sprite.
// It is called from Fruits_Draw after the background row is already drawn,
// so it only needs to write opaque sprite pixels — transparent pixels (0xFFFF)
// are skipped and the background shows through.
// Run-length merging: consecutive opaque pixels are batched into a single
// ST7735_DrawBitmap call rather than one call per pixel, which is critical
// because each SPI transaction has fixed overhead.
// imgRow: bitmaps are stored bottom-up (row 0 = bottom), so row index is inverted.
static void DrawScanline(int16_t sy,
                         const uint16_t *img, int16_t x,
                         int16_t top, int16_t w, int16_t h){
    if(sy < top || sy > top + h - 1) return;
    int16_t imgRow = (h - 1) - (sy - top);  // convert screen-top-down to image-bottom-up
    const uint16_t *rp = &img[imgRow * w];
    int16_t col = 0;
    while(col < w){
        if(rp[col] == 0xFFFF){ col++; continue; }  // skip transparent pixel
        int16_t rs = col;
        while(col < w && rp[col] != 0xFFFF) col++;  // find end of opaque run
        ST7735_DrawBitmap(x + rs, sy, rp + rs, col - rs, 1);  // send entire run at once
    }
}

static void DrawBitmapMasked(int16_t x, int16_t y,
                             const uint16_t *image, int16_t w, int16_t h){
    int16_t top = y - h + 1;
    for(int16_t sy = top; sy <= y; sy++){
        DrawScanline(sy, image, x, top, w, h);
    }
}

// Draw one horizontal strip of the dojo wood-plank floor.
// Planks are 16 px tall; joints are 1 px dark lines.
// Vertical joints are staggered (brick-bond) between odd/even plank rows.
static void Wood_DrawBgRow(int16_t sy){
    uint8_t plank = (uint8_t)(sy >> 4);        // sy / 16
    if((sy & 0xF) == 0){                        // horizontal joint
        ST7735_DrawFastHLine(0, sy, LCD1_WIDTH, WOOD_JOINT);
        return;
    }
    uint16_t bg = (plank & 1u) ? WOOD_DARK : WOOD_LIGHT;
    if((plank & 1u) == 0){
        // even plank: one vertical joint at x=80
        ST7735_DrawFastHLine(0,  sy, 80, bg);
        ST7735_DrawFastHLine(80, sy, 1,  WOOD_JOINT);
        ST7735_DrawFastHLine(81, sy, 79, bg);
    } else {
        // odd plank: joints at x=40 and x=120
        ST7735_DrawFastHLine(0,   sy, 40, bg);
        ST7735_DrawFastHLine(40,  sy, 1,  WOOD_JOINT);
        ST7735_DrawFastHLine(41,  sy, 79, bg);
        ST7735_DrawFastHLine(120, sy, 1,  WOOD_JOINT);
        ST7735_DrawFastHLine(121, sy, 39, bg);
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
        // 1. Draw wood floor background for this row
        Wood_DrawBgRow(sy);
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
}

static void Collision_Check(void){
    // Slicing only while the top button is held (Switch_In() bit 0 = 1 means released).
    if(Switch_In() & 0x01u) return;
    int sliceCount = 0;
    for(int i = 0; i < MAX_FRUITS; i++){
        if(!Fruits[i].active || Fruits[i].sliced) continue;
        int16_t fw = P[Fruits[i].type].xp, fh = P[Fruits[i].type].yp;
        int16_t cx = BladeCursor.x, cy = BladeCursor.y;
        // M = 6-pixel hit margin makes slicing feel forgiving without being trivial.
        const int16_t M = 6;
        bool hit = (cx >= Fruits[i].x - M) && (cx <= Fruits[i].x + fw + M) &&
                   (cy >= Fruits[i].y - fh - M) && (cy <= Fruits[i].y + M);
        if(!hit) continue;
        Fruits[i].hitCount++;
        if(Fruits[i].type == FRUIT_BOMB){
            // Slicing a bomb converts it to the boom (explosion) sprite, freezes it
            // in place, and starts the EXPLODE_FRAMES death countdown.
            // The life penalty fires when deathTimer reaches 0 (in Fruit_Update),
            // not immediately, so the player sees the explosion first.
            Sound_Explosion();
            Lives--;
            Fruits[i].type = FRUIT_BOOM; Fruits[i].image = selection[FRUIT_BOOM];
            Fruits[i].sliced = true; Fruits[i].deathTimer = EXPLODE_FRAMES;
            Fruits[i].vy = 0; Fruits[i].vx = 0;
            LCD2_ShowEventMsg(lang("HISS! -1 LIFE","SSSS! -1 VIDA"), 0x001Fu, 60);
        } else {
            // Increment type by 1 to switch from the whole sprite to its cut variant.
            Fruits[i].type = Fruits[i].type + 1;
            Fruits[i].image = selection[Fruits[i].type];
            Fruits[i].sliced = true;
            Fruits[i].deathTimer = 0;
            // Give the cut halves a small downward push proportional to entry speed.
            Fruits[i].vy = (int16_t)(2 + (Abs16(Fruits[i].vy) / 3));
            Sound_Slice();
            sliceCount++;
        }
    }
    if(sliceCount > 0){
        // N² scoring rewards multi-slices: 1 fruit = 1 pt, 2 = 4 pts, 3 = 9 pts.
        Score += (uint32_t)(sliceCount * sliceCount);
        if(sliceCount == 2) LCD2_ShowEventMsg(lang("PURR-FECT x2!","PERFECTO x2! "), 0x07E0u, 55);
        else if(sliceCount >= 3) LCD2_ShowEventMsg(lang("PURR-FECT MEGA!","PERFECTO MEGA!"), 0x07FFu, 55);
    }
}

// Clears only the sprite zone (y = TITLE_TEXT_Y+1 to TITLE_SPRITE_Y) then
// draws the logo centered horizontally.  Keeping the clear confined to the
// sprite zone avoids erasing the "CAT NINJA" title text above it.
static void Title_DrawLogo(uint8_t frame){
    int16_t w = TitleLogoW[frame], h = TitleLogoH[frame];
    int16_t x = (int16_t)((LCD1_WIDTH - w) / 2);
    LCD1_Select();
    ST7735_FillRect(0, TITLE_TEXT_Y + 1, LCD1_WIDTH,
                    TITLE_SPRITE_Y - TITLE_TEXT_Y + 1, ST7735_BLACK);
    ST7735_DrawBitmap(x, TITLE_SPRITE_Y, TitleLogos[frame], w, h);
}
static void Title_AnimateLogo(void){
    // Single-frame logo — drawn once at Title_Init, never needs redrawing.
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

    ST7735_SetTextColor(ST7735_RED);
    ST7735_SetCursor(9, 1);
    ST7735_OutString((char*)"CAT NINJA");

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
    ST7735_SetCursor(0, 2); ST7735_OutString((char*)lang("> Joystick=cursor", "> Joystick=cursor"));
    ST7735_SetCursor(0, 3); ST7735_OutString((char*)lang("> Hold TOP: slice!", "> Mantener TOP:cortar"));
    ST7735_SetCursor(0, 4); ST7735_OutString((char*)lang("> Fruits=pts!", "> Frutas=puntos!"));
    ST7735_SetTextColor(ST7735_RED);
    ST7735_SetCursor(0, 5); ST7735_OutString((char*)lang("> DODGE BOMBS", "> EVITA BOMBAS"));
    ST7735_SetTextColor(ST7735_WHITE);
    ST7735_SetCursor(2, 6); ST7735_OutString((char*)lang("hit=-1 life", "golpe=-1 vida"));
    ST7735_SetCursor(0, 7); ST7735_OutString((char*)lang("> Miss 3 = KO!", "> 3 fallos = fin"));
    ST7735_SetTextColor(0xFD20u);
    ST7735_SetCursor(0, 8); ST7735_OutString((char*)lang("> Gets faster over", "> Acelera con"));
    ST7735_SetCursor(2, 9); ST7735_OutString((char*)lang("time and score!", "tiempo y puntos!"));
    ST7735_SetTextColor(ST7735_CYAN);
    ST7735_SetCursor(0,10); ST7735_OutString((char*)lang("> Slide pot=volume", "> Desliz=volumen"));
    ST7735_SetCursor(0,11); ST7735_OutString((char*)lang("> Score on 2nd LCD", "> Puntos en LCD2"));
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
    ST7735_SetCursor(0,11); ST7735_OutString((char*)lang("Joystick: move cursor", "Mueve cursor"));
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
    ST7735_SetCursor(2, 8); ST7735_SetTextColor(ST7735_WHITE);
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
    ST7735_SetCursor(2, 8); ST7735_SetTextColor(ST7735_WHITE);
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
    LCD1_Select(); ST7735_FillScreen(WOOD_DARK);
    // Resuming from pause must not reset score or lives; only a fresh start should.
    if(PreviousState != (uint32_t)S_PAUSED){
        Score = 0; Lives = 3;
        LED_SetLives(3);
    }
    Cursor_Reset(80, 64);
    Fruit_Init();      // clears all fruit slots and resets GameFrames / GravCounter
    // Invalidate LCD2 shadow values so LCD2_UpdateGameplayStatus forces a full redraw.
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
    ST7735_SetCursor(7, 5); ST7735_OutString((char*)lang("-- PAWS-ED --","-- PAUSA --"));
    ST7735_SetTextColor(ST7735_CYAN);
    ST7735_SetCursor(6, 7); ST7735_OutString((char*)lang("Bot: resume","Abajo: seguir"));
    ST7735_SetCursor(7, 8); ST7735_OutString((char*)lang("Top: title","Arriba: inicio"));
}

// ─────────────────────────────────────────────────────────────────────────
// GAME OVER SCREEN
// ─────────────────────────────────────────────────────────────────────────
void GameOver_Init(void){
    Sound_Sad();
    LCD1_Select(); ST7735_FillScreen(ST7735_BLACK);
    LED_SetLives(0);
    ST7735_SetTextColor(ST7735_RED);
    ST7735_SetCursor(7, 2); ST7735_OutString((char*)lang("CAT-ASTROPHE!","GATO-STROFE!"));
    ST7735_SetTextColor(ST7735_WHITE);
    ST7735_SetCursor(8, 5); ST7735_OutString((char*)lang("Score: ","Puntos: "));
    ST7735_SetTextColor(ST7735_YELLOW); ST7735_OutUDec(Score);
    if(Score > HighScore){
        HighScore = Score;
        ST7735_SetTextColor(ST7735_GREEN);
        ST7735_SetCursor(7, 7); ST7735_OutString((char*)lang("IM-PAWS-IBLE!","INCREIBLE!   "));
    }
    ST7735_SetTextColor(ST7735_WHITE);
    ST7735_SetCursor(8, 9); ST7735_OutString((char*)lang("Best: ","Mejor: ")); ST7735_OutUDec(HighScore);
    ST7735_SetTextColor(ST7735_CYAN);
    ST7735_SetCursor(6,11); ST7735_OutString((char*)lang("Top: try again","Arriba: reiniciar"));
    LCD2ScoreShadow = 0xFFFFFFFFu; LCD2LivesShadow = 0xFF;
    LCD2LevelShadow = 0xFF;
    LCD2_UpdateGameplayStatus();
}

// ─────────────────────────────────────────────────────────────────────────
// FSM TABLE
// Next[input] index semantics (returned by each *_Input function):
//   0 = no action (stay in current state)
//   1 = primary action  (select / resume / back)
//   2 = secondary action (game-over trigger, pause→resume)
//   3 = tertiary action  (back-to-title shortcut)
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
// Each function returns the FSM Next[] index for this frame (0 = no change).
// Switch_Pressed() is edge-triggered (fires only on the first frame the
// button transitions from released to pressed) so a held button does not
// keep triggering state transitions on every frame.
// ─────────────────────────────────────────────────────────────────────────
static uint32_t Title_Input(void){
    // titleCursor + 1 maps: INFO→1 (S_INSTRUCTIONS), SETTINGS→2, PLAY→3 (S_GAMEPLAY).
    if(titleCursor != MENU_NONE && Switch_Pressed(TOP_BUTTON)) return titleCursor + 1;
    return 0;
}
static uint32_t Back_Input(void){
    if(Switch_Pressed(BOTTOM_BUTTON) || Switch_Pressed(TOP_BUTTON)) return 1;
    return 0;
}
static uint32_t Settings_Input(void){
    if(Switch_Pressed(BOTTOM_BUTTON)) return 3;  // bottom = back to title
    if(settingsCursor != MENU_NONE && Switch_Pressed(TOP_BUTTON)) return settingsCursor + 1;
    return 0;
}
static uint32_t Language_Input(void){
    if(Switch_Pressed(BOTTOM_BUTTON)) return 1;  // bottom = back to settings
    if(languageCursor != MENU_NONE && Switch_Pressed(TOP_BUTTON)){
        CurrentLanguage = languageCursor;  // commit selection before leaving
        return 1;
    }
    return 0;
}
static uint32_t Credits_Input(void){
    if(Switch_Pressed(BOTTOM_BUTTON) || Switch_Pressed(TOP_BUTTON)) return 1;
    return 0;
}
static uint32_t Gameplay_Input(void){
    if(Lives == 0) return 2;               // automatic transition to game-over
    if(Switch_Pressed(BOTTOM_BUTTON)) return 1;  // manual pause
    return 0;
}
static uint32_t Pause_Input(void){
    if(Switch_Pressed(BOTTOM_BUTTON)) return 1;  // resume gameplay
    if(Switch_Pressed(TOP_BUTTON))    return 3;  // quit to title
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
    // Sample joystick every frame regardless of state so the cursor is always
    // responsive; each state interprets the axes differently (see Cursor_Sample).
    Cursor_Sample();

    // State-change detection: InitPt runs exactly once per transition to repaint
    // the screen for the new state.  LCD2 reaction sprite also updates here.
    if(S != PreviousState){
        (FSM[S].InitPt)();
        {
            // excited_cat plays during active gameplay/menus; sleeping_cat signals
            // paused or game-over so the player gets a visual cue on the second screen.
            const unsigned short *img = excited_cat; int16_t w=89, h=65;
            if(S == S_PAUSED)        { img = sleeping_cat; w = 100; }
            else if(S == S_GAMEOVER) { img = sleeping_cat; w = 100; }
            LCD2_ShowReaction(img, w, h);
        }
        LCD1_Select();   // always return SPI mux to LCD1 after LCD2 writes
        PreviousState = S;
    }

    Cursor_UpdateColor();  // red = slicing, yellow = hovering selectable, cyan = idle

    if(S == S_TITLE){
        Title_AnimateLogo();     // no-op (single frame); kept for structural symmetry
        Title_HighlightMenu();   // redraws button labels only when selection changes
    } else if(S == S_SETTINGS){
        Settings_Highlight();
    } else if(S == S_LANGUAGE){
        Language_Highlight();
    } else if(S == S_GAMEPLAY){
        Fruit_Update();          // physics, spawning, miss detection
        Collision_Check();       // hit test cursor against all active fruits
        Fruits_Draw();           // scanline pass: background + fruits + cursor, no tear
        LED_SetLives(Lives);     // update the 3 PCB LEDs to reflect current life count
        LCD2_UpdateGameplayStatus();  // push score/level to LCD2 only if they changed
        LCD2_TickEventMsg();          // count down and clear the event banner on LCD2
    }
}
