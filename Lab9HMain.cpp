// Lab9HMain.cpp
// Runs on MSPM0G3507
// Lab 9 ECE319H
// Kaitlyn Chen
// Last Modified: April 21, 2026

//made files
#include "game.h"
#include "LCD2.h"

#include <stdio.h>
#include <stdint.h>
#include <ti/devices/msp/msp.h>
#include "../inc/ST7735.h"
#include "../inc/Clock.h"
#include "../inc/LaunchPad.h"
#include "../inc/TExaS.h"
#include "../inc/Timer.h"
#include "../inc/SlidePot.h"
#include "../inc/DAC5.h"
#include "SmallFont.h"
#include "LED.h"
#include "Switch.h"
#include "Sound.h"
#include "images/images.h"
#include "Timer.h"
#include "JoyStick.h"
#include "../inc/SPI.h"

extern "C" void __disable_irq(void);
extern "C" void __enable_irq(void);
extern "C" void TIMG12_IRQHandler(void);

extern "C" {
    #include "ADC.h" 
}
// ****note to ECE319K students****
// the data sheet says the ADC does not work when clock is 80 MHz
// however, the ADC seems to work on my boards at 80 MHz
// I suggest you try 80MHz, but if it doesn't work, switch to 40MHz
void PLL_Init(void){ // set phase lock loop (PLL)
  // Clock_Init40MHz(); // run this line for 40MHz
  Clock_Init80MHz();   // run this line for 80MHz
}

uint32_t M=1;
uint32_t Random32(void){
  M = 1664525*M+1013904223;
  return M;
}
uint32_t Random(uint32_t n){
  return (Random32()>>16)%n;
}

SlidePot Sensor(1500,0); // copy calibration from Lab 7

// games  engine runs at 30Hz
void TIMG12_IRQHandler(void){uint32_t pos,msg;
  if((TIMG12->CPU_INT.IIDX) == 1){ // this will acknowledge
    GPIOB->DOUTTGL31_0 = GREEN; // toggle PB27 (minimally intrusive debugging)
    GPIOB->DOUTTGL31_0 = GREEN; // toggle PB27 (minimally intrusive debugging)
// game engine goes here
    // 1) sample slide pot
    // 2) read input switches
    // 3) move sprites
    // 4) start sounds
    // 5) set semaphore
    // NO LCD OUTPUT IN INTERRUPT SERVICE ROUTINES
    Semaphore = 1;
    GPIOB->DOUTTGL31_0 = GREEN; // toggle PB27 (minimally intrusive debugging)
  }
}
uint8_t TExaS_LaunchPadLogicPB27PB26(void){
  return (0x80|((GPIOB->DOUT31_0>>26)&0x03));
}

typedef enum {English, Spanish, Portuguese, French} Language_t;
Language_t myLanguage=English;
typedef enum {HELLO, GOODBYE, LANGUAGE} phrase_t;
const char Hello_English[] ="Hello";
const char Hello_Spanish[] ="\xADHola!";
const char Hello_Portuguese[] = "Ol\xA0";
const char Hello_French[] ="All\x83";
const char Goodbye_English[]="Goodbye";
const char Goodbye_Spanish[]="Adi\xA2s";
const char Goodbye_Portuguese[] = "Tchau";
const char Goodbye_French[] = "Au revoir";
const char Language_English[]="English";
const char Language_Spanish[]="Espa\xA4ol";
const char Language_Portuguese[]="Portugu\x88s";
const char Language_French[]="Fran\x87" "ais";
const char *Phrases[3][4]={
  {Hello_English,Hello_Spanish,Hello_Portuguese,Hello_French},
  {Goodbye_English,Goodbye_Spanish,Goodbye_Portuguese,Goodbye_French},
  {Language_English,Language_Spanish,Language_Portuguese,Language_French}
};
// use main1 to observe special characters
int main1(void){ // main1
    char l;
  __disable_irq();
  PLL_Init(); // set bus speed
  LaunchPad_Init();
  ST7735_InitPrintf(INITR_BLACKTAB); // INITR_REDTAB for AdaFruit, INITR_BLACKTAB for HiLetGo
  ST7735_FillScreen(0x0000);            // set screen to black
  for(int myPhrase=0; myPhrase<= 2; myPhrase++){
    for(int myL=0; myL<= 3; myL++){
         ST7735_OutString((char *)Phrases[LANGUAGE][myL]);
      ST7735_OutChar(' ');
         ST7735_OutString((char *)Phrases[myPhrase][myL]);
      ST7735_OutChar(13);
    }
  }
  Clock_Delay1ms(3000);
  ST7735_FillScreen(0x0000);       // set screen to black
  l = 128;
  while(1){
    Clock_Delay1ms(2000);
    for(int j=0; j < 3; j++){
      for(int i=0;i<16;i++){
        ST7735_SetCursor(7*j+0,i);
        ST7735_OutUDec(l);
        ST7735_OutChar(' ');
        ST7735_OutChar(' ');
        ST7735_SetCursor(7*j+4,i);
        ST7735_OutChar(l);
        l++;
      }
    }
  }
}

// use main2 to observe graphics
int main2(void){ // main2
  __disable_irq();
  PLL_Init(); // set bus speed
  LaunchPad_Init();
  ST7735_InitPrintf(INITR_BLACKTAB); // INITR_REDTAB for AdaFruit, INITR_BLACKTAB for HiLetGo
  ST7735_FillScreen(ST7735_BLACK);
  // ST7735_DrawBitmap(22, 159, PlayerShip0, 18,8); // player ship bottom
  // ST7735_DrawBitmap(53, 151, Bunker0, 18,5);
  // ST7735_DrawBitmap(42, 159, PlayerShip1, 18,8); // player ship bottom
  // ST7735_DrawBitmap(62, 159, PlayerShip2, 18,8); // player ship bottom
  // ST7735_DrawBitmap(82, 159, PlayerShip3, 18,8); // player ship bottom
  // ST7735_DrawBitmap(0, 9, SmallEnemy10pointA, 16,10);
  // ST7735_DrawBitmap(20,9, SmallEnemy10pointB, 16,10);
  // ST7735_DrawBitmap(40, 9, SmallEnemy20pointA, 16,10);
  // ST7735_DrawBitmap(60, 9, SmallEnemy20pointB, 16,10);
  // ST7735_DrawBitmap(80, 9, SmallEnemy30pointA, 16,10);

  for(uint32_t t=500;t>0;t=t-5){
    SmallFont_OutVertical(t,104,6); // top left
    Clock_Delay1ms(50);              // delay 50 msec
  }
  ST7735_FillScreen(0x0000);   // set screen to black
  ST7735_SetCursor(1, 1);
  ST7735_OutString((char *)"GAME OVER");
  ST7735_SetCursor(1, 2);
  ST7735_OutString((char *)"Nice try,");
  ST7735_SetCursor(1, 3);
  ST7735_OutString((char *)"Earthling!");
  ST7735_SetCursor(2, 4);
  ST7735_OutUDec(1234);
  while(1){
  }
}

// use main3 to test switches and LEDs
int main3(void){ // main3
  __disable_irq();
  PLL_Init(); // set bus speed
  LaunchPad_Init();
  Switch_Init(); // initialize switches
  LED_Init(); // initialize LED
  while(1){
    // write code to test switches and LEDs

  }
}
// use main4 to test sound outputs
// int main4(void){ uint32_t last1=0,last2=0;
//   __disable_irq();
//   PLL_Init(); // set bus speed
//   LaunchPad_Init();
//   Switch_Init(); // initialize switches
//   LED_Init(); // initialize LED
//   Sound_Init();  // initialize sound
//   TExaS_Init(ADC0,6,0); // ADC1 channel 6 is PB20, TExaS scope
//   __enable_irq();
//   while(1){
//     uint8_t btn1 = (GPIOA->DIN31_0 >> 24) & 0x01;
//     uint8_t btn2 = (GPIOA->DIN31_0 >> 25) & 0x01;
//     if((last1 == 0)&&(btn1 == 1)){
//       Sound_Shoot();
//     }
//     if((last2 == 0)&&(btn2 == 1)){
//       Sound_Killed();
//     }
//     last1 = btn1;
//     last2 = btn2;
//   }
// }
//tests led functions 
int main8(void){
    __disable_irq();
    PLL_Init();
    LaunchPad_Init();
    LED_Init();
    __enable_irq();

    while(1){
        LED_SetLives(3);
        Clock_Delay1ms(500);
        LED_SetLives(2);
        Clock_Delay1ms(500);
        LED_SetLives(1);
        Clock_Delay1ms(500);
        LED_SetLives(0);
        Clock_Delay1ms(500);
    }
}
int main7(void){
    __disable_irq();
    PLL_Init();
    LaunchPad_Init();

    // LED pins: PB19, PB12, PB17 through ULN2003B
    IOMUX->SECCFG.PINCM[44] = 0x00000081;  // PB19
    IOMUX->SECCFG.PINCM[28] = 0x00000081;  // PB12
    IOMUX->SECCFG.PINCM[42] = 0x00000081;  // PB17
    GPIOB->DOE31_0 |= (1<<19) | (1<<12) | (1<<17);

    __enable_irq();

    // turn all on
    GPIOB->DOUTSET31_0 = (1<<19) | (1<<12) | (1<<17);

    while(1){
        GPIOB->DOUTTGL31_0 = (1<<19) | (1<<12) | (1<<17);
        Clock_Delay1ms(500);
    }
}
int main6(void){
    __disable_irq();
    PLL_Init();
    LaunchPad_Init();
    // configure PA15 as GPIO output
    IOMUX->SECCFG.PINCM[PA15INDEX] = 0x00000081;
    GPIOA->DOE31_0 |= (1<<15);
    __enable_irq();
    
    while(1){
        GPIOA->DOUTTGL31_0 = (1<<15); // toggle PA15
        Clock_Delay1ms(1); // ~500 Hz buzz
    }
}

int main9(void){
    __disable_irq();
    PLL_Init();
    LaunchPad_Init();
    ST7735_InitPrintf(INITR_BLACKTAB);
    // LCD2_Init();
    __enable_irq();

    while(1){
        SPI_SelectDisplay(SPI_DISPLAY_LCD1);
        ST7735_SetRotation(1);
        ST7735_FillScreen(ST7735_RED);
        ST7735_SetCursor(2, 2);
        ST7735_SetTextColor(ST7735_WHITE);
        ST7735_OutString((char*)"LCD1 ONLY");
        Clock_Delay1ms(1000);

        SPI_SelectDisplay(SPI_DISPLAY_LCD2);
        LCD2_FillScreen(LCD2_BLUE);
        Clock_Delay1ms(1000);
    }
}

// ALL ST7735 OUTPUT MUST OCCUR IN MAIN
// NOTE: ADC.h is already included above (wrapped in extern "C") — no second include needed.

int main(void){
    __disable_irq();
    PLL_Init();
    LaunchPad_Init();
    ST7735_InitPrintf(INITR_BLACKTAB);
    SPI_SelectDisplay(SPI_DISPLAY_LCD1);
    ST7735_SetRotation(1);
    ST7735_FillScreen(ST7735_BLACK);
    Sensor.Init();
    Switch_Init();
    LED_Init();
    Sound_Init();
    TExaS_Init(0,0,&TExaS_LaunchPadLogicPB27PB26);
    TimerG12_IntArm(80000000/30, 2);
    LCD2_Init();   // second display (portrait, PA26 CS) — must come after ST7735_InitPrintf
    Game_Init();
    __enable_irq();

    while(1){
      while(Semaphore == 0){}
      Semaphore = 0;

      // Read slide pot without resetting ADC1 (a full reset breaks the joystick).
      Sound_SetVolume(Joystick_ReadSlidePot());

      SPI_SelectDisplay(SPI_DISPLAY_LCD1);
      Game_UpdateFrame();

      uint32_t Input  = Game_GetInput();
      uint32_t nextS  = FSM[S].Next[Input];

      // Play meow whenever the player navigates to a new screen,
      // but not on game-over (Sound_Sad starts there instead).
      if(nextS != S && nextS != S_GAMEOVER){
          Sound_Meow();
      }

      S = nextS;
    }
}
