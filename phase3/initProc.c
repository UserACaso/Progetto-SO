#include "./headers/initProc.h"
#define VPN_START 0x80000
swap_t SwapTable[POOLSIZE];
void * SwapPool[POOLSIZE]; // qui possiamo mettere unsigned int, come abbiamo fatto in phase 2.
support_t Uproc[8];
state_t Stato[8];
int P3SemaphoreFlash[8];
int P3SemaphorePrinter[8];
int P3SemaphoreTerminalReceiver[8];
int P3SemaphoreTerminalTransmitter[8];
volatile unsigned int SwapTableSemaphore = 1;



void tester()
{
    for(int i = 0; i < POOLSIZE; i++)
    {
        SwapPool[i] = (RAMSTART + (64 * PAGESIZE) + (NCPU * PAGESIZE)) + (i * 0x1000);
        
        SwapTable[i].sw_asid = -1;
        SwapTable[i].sw_pageNo = -1;
        SwapTable[i].sw_pte = NULL;
    }

        for (int i = 0; i < 8; i++) /* Inizializzazione Dei Semafori di Device */
    { 
        P3SemaphoreFlash[i] = 1;
        P3SemaphorePrinter[i] = 1;
        P3SemaphoreTerminalReceiver[i] = 1;
        P3SemaphoreTerminalTransmitter[i] = 1;
    }

    
    
    //SYSCALL(CREATEPROCESS, state_t *statep, 0, support_t *supportp);
    for(int i = 0; i < 8; i++)
    {
        //Inizializzazione dello stato
        Stato[i].mie = MIE_ALL;
        Stato[i].status = (MSTATUS_MPIE_MASK  |  MSTATUS_MIE_MASK);
        Stato[i].pc_epc = UPROCSTARTADDR;
        Stato[i].reg_sp = USERSTACKTOP;
        Stato[i].cause = 0;    
        Stato[i].entry_hi = (i+1) << ASIDSHIFT;

        //Inizializzazione della support
        //Asid
        Uproc[i].sup_asid = (i+1);        

        //Context
        Uproc[i].sup_exceptContext[PGFAULTEXCEPT].pc = (memaddr)Pager;
        Uproc[i].sup_exceptContext[GENERALEXCEPT].pc = (memaddr)GeneralExceptionHandler;
        Uproc[i].sup_exceptContext[PGFAULTEXCEPT].status = MSTATUS_MPP_M;
        Uproc[i].sup_exceptContext[GENERALEXCEPT].status = MSTATUS_MPP_M;
        Uproc[i].sup_exceptContext[PGFAULTEXCEPT].stackPtr = &Uproc[i].sup_stackGen[499];
        Uproc[i].sup_exceptContext[GENERALEXCEPT].stackPtr = &Uproc[i].sup_stackGen[499];
        
        //PageTable
        for(int j = 0; j < USERPGTBLSIZE; j++)
        {
            unsigned int VPN = VPN_START + j;
            Uproc[i].sup_privatePgTbl[j].pte_entryHI = (VPN << VPNSHIFT) | ((i+1) << ASIDSHIFT);
            Uproc[i].sup_privatePgTbl[j].pte_entryLO = (DIRTYON & ~VALIDON);
        }
        int result = SYSCALL(CREATEPROCESS, &Stato[i], 0, &Uproc[i]);
        if (result < 0)
        {
            PANIC();
        }
        
    }
}
