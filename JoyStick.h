// JoyStick.h
// Sparkfun COM-09032 analog joystick driver for ECE319H Lab 9
// Kaitlyn Chen and Keerthana Mangalpally
// ECE319H Lab 9, Spring 2026
//
// Hardware connections (PCB rev with hardware fix):
//   PA15 (ADC1 ch0) - physical horizontal axis
//   PA17 (ADC1 ch2) - physical vertical axis
//
// Axes are SWAPPED in software because the joystick is mounted 90° rotated:
//   PA15 reading -> screen X (horizontal movement)
//   PA17 reading -> screen Y (vertical movement)

#ifndef JOYSTICK_H_
#define JOYSTICK_H_
#include <stdint.h>

// Initialize ADC1 dual-channel for PA15 (ch0) and PA17 (ch2).
void Joystick_Init(void);

// Read screen-X position (0-4095).
uint32_t Joystick_ReadX(void);

// Read screen-Y position (0-4095).
uint32_t Joystick_ReadY(void);

// Read both axes in one ADC burst (preferred — avoids double conversion).
// *x receives screen-X, *y receives screen-Y.
void Joystick_Read(uint32_t *x, uint32_t *y);

// Read slide pot (ADC1 ch5 / PB18) without resetting ADC1.
// Call instead of ADC1_Init+ADC1_In to avoid breaking the joystick ADC config.
uint32_t Joystick_ReadSlidePot(void);

#endif // JOYSTICK_H_
