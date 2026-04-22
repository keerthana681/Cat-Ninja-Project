#ifndef GAME_H
#define GAME_H
#include <stdint.h>

// ── FSM state indices ──────────────────────────────────────────────────────
#define S_TITLE        0
#define S_INSTRUCTIONS 1
#define S_SETTINGS     2
#define S_GAMEPLAY     3
#define S_PAUSED       4
#define S_GAMEOVER     5
#define S_LANGUAGE     6   // sub-screen: choose language
#define S_CREDITS      7   // sub-screen: credits

// ── Language constants ─────────────────────────────────────────────────────
#define LANG_ENGLISH  0
#define LANG_SPANISH  1

// Current language setting (set on the Language screen, read everywhere else)
extern uint8_t CurrentLanguage;

// ── FSM state structure ────────────────────────────────────────────────────
// Valvano-style indexed FSM: each state has an init function and 4 next-state
// indices driven by Game_GetInput().
struct State {
    void (*InitPt)(void);   // called once on state entry
    uint32_t Next[4];       // Next[0]=stay, Next[1..3]=transitions
};
typedef const struct State State_t;

// FSM array – 8 states (TITLE…CREDITS).  Keep this size in sync with game.cpp.
extern State_t FSM[8];

// Global state index and 30-Hz semaphore
extern uint32_t S;
extern volatile uint32_t Semaphore;

// ── Public API ─────────────────────────────────────────────────────────────
void     Game_Init(void);
uint32_t Game_GetInput(void);    // returns 0-3 for FSM transitions
void     Game_UpdateFrame(void); // redraw LCD; call every frame

#endif // GAME_H
