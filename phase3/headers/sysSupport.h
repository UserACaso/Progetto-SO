#ifndef SYSSUPPORT_H_INCLUDED
#define SYSSUPPORT_H_INCLUDED

#include "./initProc.h"

void GeneralExceptionHandler();
void P3SYSCALLHandler(support_t *sPtr);

void Terminate();
void WritePrinter(support_t *sPtr);
void WriteTerminal(support_t *sPtr);
void ReadTerminal(support_t *sPtr);

#endif