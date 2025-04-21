#ifndef INTERRUPTS_H_INCLUDED
#define INTERRUPTS_H_INCLUDED

#include "./initial.h"

void InterruptHandler(state_t* syscallState, unsigned int excode);
void PLTHandler(state_t* syscallState);
void DeviceHandler(int IntlineNo, int DevNo, state_t* syscallState);
void PseudoClockHandler(state_t* syscallState);
int DevNOGet(unsigned int Devices);

#endif