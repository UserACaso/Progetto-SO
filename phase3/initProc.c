#include "./headers/initProc.h"

swap_t SwapTable[POOLSIZE];
volatile unsigned int SwapMutex = 1;
void * SwapPool[POOLSIZE];

void main(){
    for(int i = 0; i < POOLSIZE; i++)
    {
        SwapPool[i] = (RAMSTART + (64 * PAGESIZE) + (NCPU * PAGESIZE)) + (i * 0x1000); 
    }
}
