#include "LED.h"
#include <ti/devices/msp/msp.h>
#include "../inc/LaunchPad.h"

// LEDs through ULN2003B: PB19, PB12, PB17

void LED_Init(void){
    IOMUX->SECCFG.PINCM[PB19INDEX] = 0x00000081;  // 44
    IOMUX->SECCFG.PINCM[PB12INDEX] = 0x00000081;  // 28
    IOMUX->SECCFG.PINCM[PB17INDEX] = 0x00000081;  // 42
    GPIOB->DOE31_0 |= (1<<19) | (1<<12) | (1<<17);
    GPIOB->DOUTCLR31_0 = (1<<19) | (1<<12) | (1<<17);
}

void LED_On(uint32_t data){
    GPIOB->DOUTSET31_0 = data;
}

void LED_Off(uint32_t data){
    GPIOB->DOUTCLR31_0 = data;
}

void LED_Toggle(uint32_t data){
    GPIOB->DOUTTGL31_0 = data;
}

void LED_SetLives(uint8_t lives){
    if(lives >= 1) GPIOB->DOUTSET31_0 = (1<<19);
    else           GPIOB->DOUTCLR31_0 = (1<<19);

    if(lives >= 2) GPIOB->DOUTSET31_0 = (1<<12);
    else           GPIOB->DOUTCLR31_0 = (1<<12);

    if(lives >= 3) GPIOB->DOUTSET31_0 = (1<<17);
    else           GPIOB->DOUTCLR31_0 = (1<<17);
}