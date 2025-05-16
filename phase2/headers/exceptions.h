#ifndef EXCEPTIONS_H_INCLUDED
#define EXCEPTIONS_H_INCLUDED

#include "initial.h"

void TLBHandler(state_t* syscallState, unsigned int cpuid);

void SYSCALLHandler(state_t* syscallState, unsigned int cpuid);
void Passeren(state_t* syscallState, pcb_PTR corrente);
void Verhogen(state_t* syscallState, pcb_PTR corrente);
void GetProcessID(state_t* syscallState, pcb_PTR corrente);
void GetSupportData(state_t* syscallState, pcb_PTR corrente);
void GetCPUTime(state_t* syscallState, pcb_PTR corrente);
void CreateProcess(state_t* syscallState, pcb_PTR corrente);
void TerminateProcess(state_t* syscallState, pcb_PTR corrente);
void WaitForClock(state_t* syscallState, pcb_PTR corrente);
void TRAPHandler(state_t* syscallState, unsigned int cpuid);
void DoIo(state_t* syscallState, pcb_PTR corrente);
void Terminator(pcb_PTR p);

#endif