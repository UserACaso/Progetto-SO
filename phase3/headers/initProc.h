#ifndef INITPROC_H_INCLUDED
#define INITPROC_H_INCLUDED

#include "../../phase2/headers/initial.h"

extern swap_t SwapTable[POOLSIZE];
extern void * SwapPool[POOLSIZE]; // qui possiamo mettere unsigned int, come abbiamo fatto in phase 2.
extern support_t Uproc[8];
extern int P3SemaphoreFlash[8];
extern int P3SemaphorePrinter[8];
extern int P3SemaphoreTerminalReceiver[8];
extern int P3SemaphoreTerminalTransmitter[8];
extern volatile unsigned int SwapTableSemaphore = 1;
extern void GeneralTLBHandler();
extern void GeneralExceptionHandler();
#endif