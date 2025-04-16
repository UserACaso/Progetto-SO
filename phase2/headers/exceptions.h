#ifndef EXCEPTIONS_H_INCLUDED
#define EXCEPTIONS_H_INCLUDED

#include "initial.h"

void TLBHandler();
void TRAPHandler();

void SYSCALLHandler(state_t* syscallState, unsigned int cpuid);
void Passeren(state_t* syscallState, pcb_PTR corrente);
void Verhogen(state_t* syscallState, pcb_PTR corrente);
void WaitForClock(state_t* syscallState, pcb_PTR corrente);

#endif