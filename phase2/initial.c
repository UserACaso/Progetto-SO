#include "./headers/initial.h"


int Process_Count;
struct list_head Ready_Queue;
pcb_PTR Current_Process[NCPU]; // da inizializzare a NULL
int SemaphoreDisk[8];
int SemaphoreFlash[8];
int SemaphoreNetwork[8];
int SemaphorePrinter[8];
int SemaphoreTerminal[16];
int SemaphorePseudo;
unsigned volatile int Global_Lock;

void exceptionHandler() {
    unsigned int cause = getCAUSE();
    unsigned int excode = cause & CAUSE_EXCCODE_MASK; // estraiamo eccezione tramite and con la maschera
    // Interrupt?     Exception code (excode)
    //     0          000101010101010101010101010101010

    if (CAUSE_IS_INT(cause)) {
        InterruptHandler(excode);
    }else{
        switch (excode) 
        {
            case 24 ... 28:  //For exception codes 24-28 (TLB exceptions), processing should be passed along to your Nucleus’s TLB exception handler
                TLBHandler();
                break;
            case 8:   //For exception codes 8 and 11, processing should be passed along to your Nucleus’s SYSCALL exception handler
                SYSCALLHandler(GET_EXCEPTION_STATE_PTR(getPRID()), getPRID());
                break;
            case 11:
                SYSCALLHandler(GET_EXCEPTION_STATE_PTR(getPRID()), getPRID());
                break;
            default:   //For other exception codes 0-7, 9, 10, 12-23 (Program Traps), processing should be passed along to your Nucleus’s Program Trap exception handler
                TRAPHandler(GET_EXCEPTION_STATE_PTR(getPRID()), getPRID()); 
                break;
        }
    }
}

// From gcc/libgcc/memcpy.c
void *memcpy(void *dest, const void *src, unsigned int len)
{
  char *d = dest;
  const char *s = src;
  while (len--)
    *d++ = *s++;
  return dest;
}


int main(){
 	Process_Count = 0;
  	Global_Lock = 0;
    for (int cpu_id = 0; cpu_id < NCPU; cpu_id++)
    {
        passupvector_t *passupvector;
        //in realta qua ci andrebbe un ciclo for per inizializzare tutti i processori
        //dato che viene lanciato una sola volta il main e quindi bisogna imporstare manualmente il passup vector per ogni processore
        passupvector = (passupvector_t *)(0x0FFFF900 + (cpu_id * 0x10));
        passupvector->tlb_refill_handler = (memaddr)uTLB_RefillHandler;

        if (cpu_id == 0) {
            passupvector->tlb_refill_stackPtr= KERNELSTACK;
            passupvector->exception_stackPtr = KERNELSTACK;
        } else {
            passupvector->tlb_refill_stackPtr = (cpu_id * PAGESIZE) + (64 * PAGESIZE) + RAMSTART;
            passupvector->exception_stackPtr = 0x20020000 + (cpu_id * PAGESIZE);
            //RAMSTART + (64 * PAGESIZE) + (cpu_id * PAGESIZE)
        }
        passupvector->exception_handler = (memaddr)exceptionHandler;
    }
    
    
    mkEmptyProcQ(&Ready_Queue);
    for (int i = 0; i < NCPU; i++)
    {
        Current_Process[i] = NULL;
    }
    initASL();
    initPcbs();
    Current_Process[0] = NULL; //inizializzo il puntatore al processo corrente a NULL

    //Inizializzazione liste dei processi bloccanti per ogni semaforo
    for (int i = 0; i < 8; i++)
    {
        SemaphoreFlash[i] = 0;
        SemaphoreDisk[i] = 0;
        SemaphoreNetwork[i] = 0;
        SemaphorePrinter[i] = 0;
    }
    for (int i = 0; i < 16; i++)
    {
        SemaphoreTerminal[i] = 0;
    }
    SemaphorePseudo = 0;
    LDIT(PSECOND); //imposta l'intervallo di tempo a 100ms
    
    //Istanzia il primo processo
    pcb_t *first = allocPcb();
    insertProcQ(&Ready_Queue, first);
    ++Process_Count;
    first->p_s.mie = MIE_ALL;
    first->p_s.status = (MSTATUS_MIE_MASK | MSTATUS_MPP_M);
    RAMTOP(first->p_s.reg_sp); // da 0x20001000 (Kernelstack) in su, ma lo stack cresce verso il basso
    first->p_s.pc_epc = (memaddr) test;

    // Try this approach for IRT initialization
    for (int i = 0; i < IRT_NUM_ENTRY; i++) {
        memaddr irt_addr;
        if (i == 0) {
            irt_addr = IRT_START;
        } else if (i == 1) {
            irt_addr = IRT_START + 0x20;  // Special jump for second entry
        } else {    
            irt_addr = IRT_START + 0x20 + ((i-1) * 0x4);  // Regular pattern after
        }
        //(?) 0-5, 6-12, 13-19 ... ;cpu 0, cpu 1, cpu 2, ... ; bit 0 a 1, bit 1 a 1, bit 2 a 1 ...
        *((memaddr *)irt_addr) = IRT_RP_BIT_ON | ((1 << NCPU) - 1);
    }
    

    //per le NCPU-1:
    
    for(int cpu_id = 1; cpu_id < NCPU; cpu_id++)
    {
        state_t otherCPU; //creo uno stato per ogni processore (escluso CPU0)
        otherCPU.status = MSTATUS_MPP_M;  
        otherCPU.pc_epc = (memaddr)scheduler;
        otherCPU.reg_sp = 0x20020000 + (cpu_id * PAGESIZE);
        otherCPU.cause = 0;
        otherCPU.mie = 0;
        otherCPU.entry_hi = 0;
        INITCPU(cpu_id, &otherCPU); //accendo la cpu con indirizzo cpu_id;
    }
    *((memaddr *)TPR) = 0;
    scheduler();
    
    return 0; //non dovrebbe mai raggiungerlo
}