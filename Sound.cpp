// Sound.cpp
// 5-bit DAC audio driver for Cat Ninja (ECE319H Lab 9)
// Supports one-shot and looping playback with slide-pot volume control.
#include <stdint.h>
#include <ti/devices/msp/msp.h>
#include "Sound.h"
#include "sounds/sounds_trimmed.h"
#include "../inc/DAC5.h"

// ── Playback state (written from main, read from ISR) ─────────────────────
static const uint8_t *SoundPt    = 0;
static uint32_t       SoundCount = 0;
static uint32_t       SoundIndex = 0;
static bool           SoundLoop  = false;

// ── Volume (0–31). Written from main thread, read in ISR. ─────────────────
// uint8_t write/read is atomic on ARM Cortex-M, so no critical section needed.
static uint8_t SoundVolume = 31;   // default: full volume

// ── SysTick ISR – fires at 11 kHz ─────────────────────────────────────────
extern "C" void SysTick_Handler(void);
void SysTick_Handler(void){
    if(!SoundPt) return;

    if(SoundIndex >= SoundCount){
        if(SoundLoop){
            SoundIndex = 0;     // restart for looping sounds
        } else {
            DAC5_Out(0);        // silence when one-shot finishes
            return;
        }
    }

    // Sounds are stored as 8-bit PCM; shift to 5-bit then scale by volume.
    // raw  : 0–31 (5-bit)
    // out  : 0–31 (volume-scaled 5-bit)
    uint8_t raw = SoundPt[SoundIndex] >> 3;
    DAC5_Out((uint8_t)((raw * SoundVolume) >> 5));
    SoundIndex++;
}

// ── Public API ─────────────────────────────────────────────────────────────
void Sound_Init(void){
    DAC5_Init();
    SoundPt     = 0;
    SoundCount  = 0;
    SoundIndex  = 0;
    SoundLoop   = false;
    SoundVolume = 31;

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
    // 12-bit ADC (0–4095) → 5-bit volume (2–31); floor at 2 so sound is audible
    // even when the slide pot is near minimum.
    uint8_t v = (uint8_t)(adcVal >> 7);
    SoundVolume = (v < 2) ? 2 : v;
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
    // Loops continuously — call Sound_Stop() to silence it.
    SoundPt    = sad;
    SoundCount = sizeof(sad);
    SoundIndex = 0;
    SoundLoop  = true;
}

void Sound_Meow(void){
    Sound_Start(meow, sizeof(meow));
}

void Sound_Explosion(void){
    Sound_Start(explosion, sizeof(explosion));
}
