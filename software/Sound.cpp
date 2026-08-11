// Sound.cpp
// 5-bit DAC audio driver for Cat Ninja (ECE319H Lab 9)
// Supports one-shot and looping playback with slide-pot volume control.
#include <stdint.h>
#include <ti/devices/msp/msp.h>
#include "Sound.h"
#include "sounds/sounds.h"
#include "../inc/DAC5.h"

// ── Playback state ────────────────────────────────────────────────────────
// Written from main thread (Sound_Start/Stop/Sad), read every tick by the
// SysTick ISR.  Pointer assignment and 32-bit counter reads are not atomic
// on Cortex-M0+, but in practice only one sound plays at a time and a
// partial update at worst produces a single garbage sample, which is inaudible.
static const uint8_t *SoundPt    = 0;     // pointer to active sample array; 0 = silent
static uint32_t       SoundCount = 0;     // total number of samples in the array
static uint32_t       SoundIndex = 0;     // index of the next sample to output
static bool           SoundLoop  = false; // true = restart from index 0 after last sample

// ── Volume ────────────────────────────────────────────────────────────────
// Ranges 0–32: 0 = muted, 32 = full amplitude (raw sample passes through unchanged).
// uint8_t reads and writes are single-instruction on Cortex-M, so no mutex needed
// for this shared variable between main and the ISR.
static uint8_t SoundVolume = 32;

// ── SysTick ISR — fires at 11,025 Hz ──────────────────────────────────────
// SysTick is loaded with 80 MHz / 7256 ≈ 11,025 counts, matching the standard
// audio sample rate used when the PCM arrays were extracted from .wav files.
// The ISR must return quickly: it reads one byte, multiplies, and calls DAC5_Out.
extern "C" void SysTick_Handler(void);
void SysTick_Handler(void){
    if(!SoundPt) return;  // no active sound; ISR costs one comparison and exits

    if(SoundIndex >= SoundCount){
        if(SoundLoop){
            SoundIndex = 0;       // loop: restart from the beginning
        } else {
            DAC5_Out(0);          // one-shot finished: drive DAC to midpoint (silence)
            return;
        }
    }

    // Samples are 5-bit unsigned PCM (0–31).  Scale by volume then output.
    // volume=32: (raw * 32) >> 5 = raw — full amplitude, no scaling overhead.
    // volume=0:  output is 0 — silent.
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

    // Configure SysTick: clear control, load reload value, clear current value,
    // then enable with processor clock source (bit 2) and interrupt (bit 1).
    SysTick->CTRL = 0;
    SysTick->LOAD = 7256 - 1;   // 80 MHz / 7256 ≈ 11,025 Hz sample rate
    SysTick->VAL  = 0;
    SysTick->CTRL = 0x07;       // CLKSOURCE=1 (core clock), TICKINT=1, ENABLE=1
}

// Begin one-shot playback.  Interrupts are not disabled because writing
// SoundPt last ensures the ISR either uses the old buffer entirely or switches
// to the new one at the next sample boundary — never an invalid mid-buffer state.
void Sound_Start(const uint8_t *pt, uint32_t count){
    SoundLoop  = false;
    SoundCount = count;
    SoundIndex = 0;
    SoundPt    = pt;   // written last so ISR sees a consistent state
}

// Maps the 12-bit slide-pot ADC value (0–4095) to volume (0–32).
// Multiplying by 33 then shifting right 12 gives a ceiling of exactly 32
// at ADC max (4095 * 33 >> 12 = 32.99 → 32 after saturation).
void Sound_SetVolume(uint32_t adcVal){
    uint32_t v = (adcVal * 33U) >> 12;
    SoundVolume = (uint8_t)(v > 32U ? 32U : v);
}

// Immediately silence output and release the buffer pointer.
// After this returns, the next SysTick ISR sees SoundPt == 0 and exits early.
void Sound_Stop(void){
    SoundPt    = 0;
    SoundLoop  = false;
    SoundCount = 0;
    SoundIndex = 0;
    DAC5_Out(0);
}

// ── Game sounds ────────────────────────────────────────────────────────────
// Sound_Sad loops the explosion buffer continuously as a distress tone on
// the game-over screen.  All other sounds are one-shot.
void Sound_Sad(void){
    SoundCount = sizeof(explosion);
    SoundIndex = 0;
    SoundLoop  = true;
    SoundPt    = explosion;
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
