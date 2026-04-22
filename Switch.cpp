/*
 * Switch.cpp
 *
 *  Created on: Nov 5, 2023
 *      Author:
 */
#include "Switch.h"
#include <ti/devices/msp/msp.h>
#include "../inc/LaunchPad.h"

static uint8_t prevTop = 1, prevBottom = 1;

void Switch_Init(void){
    // Match the current PCB wiring:
    //   top/select   -> PA24
    //   bottom/pause -> PA18
    IOMUX->SECCFG.PINCM[PA24INDEX] = 0x00060081; // GPIO input with pull-up, top button
    IOMUX->SECCFG.PINCM[PA18INDEX] = 0x00060081; // GPIO input with pull-up, bottom button
}

uint32_t Switch_In(void){
    uint32_t top = ((GPIOA->DIN31_0 >> 24) & 0x01) ? 0 : 1;
    uint32_t bottom = ((GPIOA->DIN31_0 >> 18) & 0x01) ? 0 : 1;
    return (bottom << 1) | top;
}

uint8_t Switch_Pressed(uint8_t button){
    if(button == TOP_BUTTON){
        uint8_t curr = (GPIOA->DIN31_0 >> 24) & 0x01;  // PA24, active-low
        uint8_t pressed = (prevTop == 1) && (curr == 0);
        prevTop = curr;
        return pressed;
    }
    if(button == BOTTOM_BUTTON){
        uint8_t curr = (GPIOA->DIN31_0 >> 18) & 0x01;  // PA18, active-low
        uint8_t pressed = (prevBottom == 1) && (curr == 0);
        prevBottom = curr;
        return pressed;
    }
    return 0;
}
