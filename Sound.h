// Sound.h
// Runs on MSPM0
// Play sounds on 5-bit DAC.
#ifndef SOUND_H
#define SOUND_H
#include <stdint.h>

// Initialize SysTick at 11 kHz and the 5-bit DAC. Call once at startup.
void Sound_Init(void);

// Low-level: load a sound buffer; plays once then stops.
void Sound_Start(const uint8_t *pt, uint32_t count);

// Set output volume from a raw 12-bit ADC reading (0–4095).
// Maps linearly to the 5-bit DAC range (0–31).
// Call every frame using the slide-pot ADC value.
void Sound_SetVolume(uint32_t adcVal);

// Stop any playing sound immediately and silence the DAC.
void Sound_Stop(void);

// Game sounds -----------------------------------------------------------
void Sound_Sad(void);       // sad music, loops until Sound_Stop()
void Sound_Meow(void);      // short meow, plays once
void Sound_Explosion(void); // bomb explosion, plays once
void Sound_Slice(void);     // fruit slice, plays once

#endif
