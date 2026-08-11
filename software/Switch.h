/*
 * Switch.h
 *
 *  Created on: Nov 5, 2023
 *      Author: jonat
 */

#ifndef SWITCH_H_
#define SWITCH_H_
#include <stdint.h>

#define TOP_BUTTON 0
#define BOTTOM_BUTTON 1

// initialize your switches
void Switch_Init(void);

// return current state of switches
uint32_t Switch_In(void);

// returns 1 if newly pressed
uint8_t Switch_Pressed(uint8_t button);
#endif /* SWITCH_H_ */
