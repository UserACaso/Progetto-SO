#ifndef INITIAL_H_INCLUDED
#define INITIAL_H_INCLUDED

#include <uriscv/liburiscv.h>
#include <uriscv/cpu.h>
#include <uriscv/types.h>
#include "../../phase1/headers/asl.h"
#include "../../phase1/headers/pcb.h"
#include "./interrupts.h"
#include "./exceptions.h"
#include "./scheduler.h"

extern void test();
extern void uTLB_RefillHandler();
extern int Process_Count;
extern struct list_head Ready_Queue;
extern pcb_PTR Current_Process[NCPU]; // da inizializzare a NULL
extern int SemaphoreDisk[8];
extern int SemaphoreFlash[8];
extern int SemaphoreNetwork[8];
extern int SemaphorePrinter[8];
extern int SemaphoreTerminalReceiver[8];
extern int SemaphoreTerminalTransmitter[8];
extern int SemaphorePseudo;
extern unsigned volatile int Global_Lock;
extern volatile cpu_t Timestamp[8];
extern void *memcpy(void *dest, const void *src, unsigned int len);
void exceptionHandler();

#endif