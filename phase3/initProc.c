#include "./headers/initProc.h"

swap_t SwapTable[POOLSIZE];
volatile unsigned int SwapMutex = 1;
void * SwapPool[POOLSIZE]; // qui possiamo mettere unsigned int, come abbiamo fatto in phsae 2.

void main(){
    for(int i = 0; i < POOLSIZE; i++)
    {
        SwapPool[i] = (RAMSTART + (64 * PAGESIZE) + (NCPU * PAGESIZE)) + (i * 0x1000);
        
        
        SwapTable[i].sw_asid = -1;
        SwapTable[i].sw_pageNo = -1;
        SwapTable[i].sw_pte = NULL;
    }

  
}
