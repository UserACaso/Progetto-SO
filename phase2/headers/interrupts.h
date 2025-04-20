#ifndef INTERRUPTS_H_INCLUDED
#define INTERRUPTS_H_INCLUDED

#include "./initial.h"

void InterruptHandler(state_t* syscallState, unsigned int excode);
void PLTHandler();
void DeviceHandler(int IntlineNo, int DevNo);
void PseudoClockHandler();
int DevNOGet(unsigned int Devices);

#endif