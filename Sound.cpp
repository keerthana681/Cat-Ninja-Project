// Sound.cpp
// 5-bit DAC audio driver for Cat Ninja (ECE319H Lab 9)
// Supports one-shot and looping playback with slide-pot volume control.
#include <stdint.h>
#include <ti/devices/msp/msp.h>
#include "Sound.h"
#include "sounds/sounds.h"
#include "../inc/DAC5.h"

// ── Playback state (written from main, read from ISR) ─────────────────────
static const uint8_t *SoundPt    = 0;
static uint32_t       SoundCount = 0;
static uint32_t       SoundIndex = 0;
static bool           SoundLoop  = false;

// ── Volume (0–32). Written from main thread, read in ISR. ─────────────────
// 32 = full amplitude (raw passes through unchanged). uint8_t r/w is atomic on Cortex-M.
static uint8_t SoundVolume = 32;   // default: full volume

// ── SysTick ISR – fires at 11 kHz ─────────────────────────────────────────
extern "C" void SysTick_Handler(void);
void SysTick_Handler(void){
    if(!SoundPt) return;

    if(SoundIndex >= SoundCount){
        if(SoundLoop){
            SoundIndex = 0;
        } else {
            DAC5_Out(0);
            return;
        }
    }

    // Samples are already 5-bit (0-31) — output directly, scaled by volume.
    // volume=32 → raw passes through at full amplitude (matching Lab5 approach).
    uint8_t raw = SoundPt[SoundIndex++];
    DAC5_Out((uint8_t)((uint32_t)raw * SoundVolume >> 5));
}

// ── Public API ─────────────────────────────────────────────────────────────
void Sound_Init(void){
    DAC5_Init();
    SoundPt     = 0;
    SoundCount  = 0;
    SoundIndex  = 0;
    SoundLoop   = false;
    SoundVolume = 32;

    SysTick->CTRL = 0;
    SysTick->LOAD = 7256 - 1;   // 80 MHz / 7256 ≈ 11 025 Hz
    SysTick->VAL  = 0;
    SysTick->CTRL = 0x07;       // enable SysTick with processor clock + interrupt
}

void Sound_Start(const uint8_t *pt, uint32_t count){
    SoundLoop  = false;
    SoundPt    = pt;
    SoundCount = count;
    SoundIndex = 0;
}

void Sound_SetVolume(uint32_t adcVal){
    // 12-bit ADC (0–4095) → volume 0–32.
    // At slide-pot max: volume=32 → DAC output = raw sample (full amplitude).
    // Multiply by 33 so the top of the range reaches 32 cleanly.
    uint32_t v = (adcVal * 33U) >> 12;   // 0–33; saturate at 32
    SoundVolume = (uint8_t)(v > 32U ? 32U : v);
}

void Sound_Stop(void){
    SoundLoop  = false;
    SoundPt    = 0;
    SoundCount = 0;
    SoundIndex = 0;
    DAC5_Out(0);
}

// ── Game sounds ────────────────────────────────────────────────────────────
void Sound_Sad(void){
    SoundPt    = explosion;
    SoundCount = sizeof(explosion);
    SoundIndex = 0;
    SoundLoop  = true;
}

void Sound_Meow(void){
    Sound_Start(meow, sizeof(meow));
}

void Sound_Explosion(void){
    Sound_Start(explosion, sizeof(explosion));
}

void Sound_Slice(void){
    Sound_Start(slice, sizeof(slice));
}
