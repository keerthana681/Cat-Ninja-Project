# Cat Ninja Handheld Console Game
 
A handheld gaming console built from scratch for UT Austin's ECE319H (Introduction to Embedded Systems), featuring a custom PCB, dual-LCD displays, and an interrupt-driven game engine.
 
## Overview
 
Cat Ninja is a fully custom embedded gaming handheld built around the TI MSPM0 microcontroller. The project spans the full hardware-software stack: a custom-designed PCB, analog input sampling via joystick and slide potentiometer, a discrete DAC for in-game sound, and a real-time game engine driven by hardware timers and interrupts.
 
## Features
 
- **Custom PCB** designed in KiCad, integrating the TI MSPM0 microcontroller with dual-LCD displays and a joystick interface
- **Dual-LCD setup** — one screen renders the live game, the other displays the score and a reactive cat sprite (happy when fish are sliced, sad when a bomb is hit or a life is lost)
- **Original hand-drawn sprite art** for the cat cursor, fish, sliced-fish states, and bombs
- **Analog input sampling** via ADC for the joystick (menu navigation and cursor movement) and slide potentiometer (in-game volume control)
- **5-bit binary-weighted DAC** for real-time sound effects (slicing, explosions, and more)
- **Interrupt-driven game engine** written in C++, using SysTick and TimerG12 for periodic game logic, rendering, and input polling
- **Finite state machine (FSM) architecture** driving the title menu, settings, gameplay, pause, and game-over screens
## How to Play
 
**Menu Navigation**
- On the title screen, move the joystick to switch between **Info**, **Settings**, and **Play**
- One button **selects** the highlighted option; the other button **goes back** to the previous screen
- **Info** explains how to play
- **Settings** lets you change the display language (English/Spanish)
**Gameplay**
- Move the cat paw cursor around the screen using the joystick
- Press the select button to **slice** — this triggers a slicing sound and swaps the fish sprite to its sliced state
- Fish rise and fall across the screen at random; slice them to score points
- Bombs appear at random alongside the fish — avoid slicing these
- You have **3 lives**. You lose a life if you let a fish fall unsliced, or if you slice a bomb (which also triggers an explosion sound)
- When you run out of lives, the game ends with a crying-cat sound effect
- The volume of in-game sound effects is controlled in real time by the slide potentiometer
## Hardware
 
- TI MSPM0 microcontroller
- Dual-LCD display setup
- Joystick + slide potentiometer analog inputs
- Custom binary-weighted DAC circuit for audio output
- PCB designed and fabricated for this project (see `/hardware` for KiCad files and schematics)
## Software
 
- Core game engine written in C++
- SysTick and TimerG12 used for periodic, interrupt-driven task scheduling
- ADC driver for analog input sampling
- DAC output driver for sound playback
## Team
 
Built by Keerthana Mangalpally and Kaitlyn Chen for ECE319H at UT Austin.
 
## Acknowledgments & License
 
This project uses hardware driver code adapted from course materials provided by Dr. Jonathan Valvano for UT Austin's embedded systems curriculum, used under the license below.
 
```
Simplified BSD License (FreeBSD License)
Copyright © 2025, Jonathan Valvano, All rights reserved.
 
Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:
 
1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 
The views and conclusions contained in the software and documentation are
those of the authors and should not be interpreted as representing official
policies, either expressed or implied, of the FreeBSD Project.
```
 
For more information on the original course materials, see [Dr. Valvano's course page](http://users.ece.utexas.edu/~valvano/).
