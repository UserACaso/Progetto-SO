#include "./headers/initial.h"

// Variabili Globali
int Process_Count;                      // Contatore del numero totale di processi attivi
struct list_head Ready_Queue;           // Coda di processi pronti per l'esecuzione
pcb_PTR Current_Process[NCPU];          // Array of pointers to currently executing process on each CPU
int SemaphoreDisk[8];                   // Semaphores for disk devices (one per device)
int SemaphoreFlash[8];                  // Semaphores for flash devices
int SemaphoreNetwork[8];                // Semaphores for network devices
int SemaphorePrinter[8];                // Semaphores for printer devices
int SemaphoreTerminalReceiver[8];       // Semaphores for terminal receiving
int SemaphoreTerminalTransmitter[8];    // Semaphores for terminal transmitting
int SemaphorePseudo;                    // Semaphore for the Pseudo-clock
unsigned volatile int Global_Lock;      // Global lock per le operazioni sui multi processori
volatile cpu_t Timestamp[8];            // Time for each process in the cpu

// Dalla gcc/libgcc/memcpy.c
void *memcpy(void *dest, const void *src, unsigned int len)
{
  char *d = dest;
  const char *s = src;
  while (len--)
    *d++ = *s++;
  return dest;
}

void exceptionHandler() {
    unsigned int cause = getCAUSE();
    unsigned int excode = cause & CAUSE_EXCCODE_MASK; // estraiamo eccezione tramite and con la maschera
    // Interrupt?     Exception code (excode)
    //     0          000101010101010101010101010101010

    if (CAUSE_IS_INT(cause)) {
        
        InterruptHandler(GET_EXCEPTION_STATE_PTR(getPRID()), excode);
    }else{
        switch (excode) 
        {
            case 24 ... 28: /* Per le eccezioni 24-28 (TLB exceptions), passiamo il controllo al TBL Handler */
                TLBHandler(GET_EXCEPTION_STATE_PTR(getPRID()), getPRID());
                break;
            case 8:   /* Per le eccezzioni 8 e 11,  passiamo il controllo al Syscall handler */
                SYSCALLHandler(GET_EXCEPTION_STATE_PTR(getPRID()), getPRID());
                break;
            case 11:
                SYSCALLHandler(GET_EXCEPTION_STATE_PTR(getPRID()), getPRID());
                break;
            default:   //Per le eccezzioni 0-7, 9, 10, 12-23 (Program Traps), passiamo il controllo al Trap handler
                TRAPHandler(GET_EXCEPTION_STATE_PTR(getPRID()), getPRID()); 
                break;
        }
    }
}

int main(){


    
    /* INIZIALIZZAZIONE VARIABILI GLOBALI */

    Process_Count = 0;
    Global_Lock = 0;
    SemaphorePseudo = 0;

    
    for (int cpu_id = 0; cpu_id < NCPU; cpu_id++) /* Inizializzazione campi dei Passupvector */
    { 
        passupvector_t *passupvector;
        passupvector = (passupvector_t *)(0x0FFFF900 + (cpu_id * 0x10));
        passupvector->tlb_refill_handler = (memaddr)uTLB_RefillHandler;

        if (cpu_id == 0) {
            passupvector->tlb_refill_stackPtr= KERNELSTACK;
            passupvector->exception_stackPtr = KERNELSTACK;
        } else {
            passupvector->tlb_refill_stackPtr = (cpu_id * PAGESIZE) + (64 * PAGESIZE) + RAMSTART;
            passupvector->exception_stackPtr = 0x20020000 + (cpu_id * PAGESIZE);
        }
        passupvector->exception_handler = (memaddr)exceptionHandler;
    }
    
    
    mkEmptyProcQ(&Ready_Queue); /* Inizializzazione ReadyQueue */
    
    for (int i = 0; i < NCPU; i++)
    {
        Current_Process[i] = NULL;
        Timestamp[i]= 0;
    }
    
    initASL(); /* Inizializza lista Semafori disponibili */
    initPcbs(); /* Iniizalizza  lista PCB disponibili*/

    for (int i = 0; i < 8; i++) /* Inizializzazione Dei Semafori di Device */
    { 
        SemaphoreFlash[i] = 0;
        SemaphoreDisk[i] = 0;
        SemaphoreNetwork[i] = 0;
        SemaphorePrinter[i] = 0;
        SemaphoreTerminalReceiver[i] = 0;
        SemaphoreTerminalTransmitter[i] = 0;
    }
    
    LDIT(PSECOND); /* Imposta l'IntervalTimer a 100ms */



    /* ISTANZIA DEL 1° PROCESSO */

    pcb_t *first = allocPcb();
    insertProcQ(&Ready_Queue, first); /* Inserimento del primo processo nella ReadyQueue */
    ++Process_Count;
    first->p_s.mie = MIE_ALL;
    first->p_s.status = (MSTATUS_MIE_MASK | MSTATUS_MPP_M);
    
    RAMTOP(first->p_s.reg_sp); /* Per istanziare lo stato del primo processo, attiviamo le Interrupt in Kernel Mode cohn RAMTOP */
    first->p_s.pc_epc = (memaddr) test;

    for (int i = 0; i < IRT_NUM_ENTRY; i++) { /* Inizializzazione Interrupt Routing Table */
        memaddr irt_addr;
        if (i == 0)
            irt_addr = IRT_START; /* L'Interrupt Routing Table parte dalla Linea 2 */
        else 
            irt_addr = IRT_START + 0x20 + ((i-1) * 0x4); /* Altre Interrupt line */

        *((memaddr *)irt_addr) = IRT_RP_BIT_ON | ((1 << NCPU) - 1); /* Accensione bit RP */
    }   


    /* INIZIALIZZAZIONE DELLE RESTANTI CPU */
       
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
    *((memaddr *)TPR) = 0; //deve essere settato per ogni singolo processore
    scheduler();
    klog_print("rompo");
}