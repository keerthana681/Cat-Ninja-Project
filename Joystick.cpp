// Joystick.cpp
// Sparkfun COM-09032 analog joystick driver
// Kaitlyn Chen and Keerthana Mangalpally
// ECE319H Lab 9, Spring 2026
//
// PCB wiring (after hardware fix):
//   Physical horizontal stick -> PA17 (ADC1 channel 2)
//   Physical vertical stick   -> PA15 (ADC1 channel 0)
//   Slide pot                 -> PB18 (ADC1 channel 5)
//
// ADC1 strategy: init ONCE in Joystick_Init with all three channels loaded
// into separate MEMCTLn registers.  Never reset ADC1 again after that.
//   MEMCTL[0] = ch5  (slide pot, PB18)   -- read via Joystick_ReadSlidePot
//   MEMCTL[1] = ch0  (joystick PA15)     \  read via ADC_InDual -> MEMRES[1/2]
//   MEMCTL[2] = ch2  (joystick PA17)     /

#include <stdint.h>
#include <ti/devices/msp/msp.h>
#include "JoyStick.h"
#include "../inc/LaunchPad.h"

extern "C" {
    #include "ADC.h"
}

// CTL2 value for dual-channel sequencer (STARTADD=1, ENDADD=2)
#define CTL2_DUAL  0x02010000u
// CTL2 value for single-channel (STARTADD=0, ENDADD=0)
#define CTL2_SINGLE 0x00000000u

void Joystick_Init(void){
    IOMUX->SECCFG.PINCM[PA15INDEX] = 0x00040081; // PA15 analog (ADC1 ch0)
    IOMUX->SECCFG.PINCM[PA17INDEX] = 0x00040081; // PA17 analog (ADC1 ch2)
    // Full one-time init: dual channel for joystick (MEMCTL[1]/[2])
    ADC_InitDual(ADC1, 0, 2, ADCVREF_VDDA);
    // Pre-load MEMCTL[0] with slide pot channel 5 (PB18).
    // IOMUX for PB18 was already configured by SlidePot::Init() in main.
    ADC1->ULLMEM.MEMCTL[0] = ADCVREF_VDDA + 5;
}

// Read both joystick axes.  Never resets ADC1 — just triggers a dual conversion.
void Joystick_Read(uint32_t *x, uint32_t *y){
    uint32_t physHoriz, physVert;
    ADC1->ULLMEM.CTL2 = CTL2_DUAL;         // ensure sequencer covers MEMCTL[1..2]
    ADC_InDual(ADC1, &physHoriz, &physVert);
    if(x) *x = 4095 - physHoriz;           // invert X to match screen orientation
    if(y) *y = physVert;
}

// Read slide pot (ADC1 ch5) without resetting the ADC.
// Temporarily sequences only MEMCTL[0] (ch5), reads result, restores dual mode.
uint32_t Joystick_ReadSlidePot(void){
    ADC1->ULLMEM.CTL2 = CTL2_SINGLE;
    ADC1->ULLMEM.CTL0 |= 0x00000001u;                      // enable
    ADC1->ULLMEM.CTL1 |= 0x00000100u;                      // start
    uint32_t volatile delay = ADC1->ULLMEM.STATUS;          // let ADC start
    while((ADC1->ULLMEM.STATUS & 0x01u) == 0x01u){}        // wait done
    uint32_t result = ADC1->ULLMEM.MEMRES[0];
    ADC1->ULLMEM.CTL2 = CTL2_DUAL;                         // restore dual mode
    return result;
}

uint32_t Joystick_ReadX(void){
    uint32_t x, y;
    Joystick_Read(&x, &y);
    return x;
}

uint32_t Joystick_ReadY(void){
    uint32_t x, y;
    Joystick_Read(&x, &y);
    return y;
}
