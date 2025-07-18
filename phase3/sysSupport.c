#include "./headers/sysSupport.h"

void GeneralExceptionHandler() {
    support_t *sPtr = SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    switch (sPtr->sup_exceptState[GENERALEXCEPT].cause & CAUSE_EXCCODE_MASK)
    {
    case 8:
        P3SYSCALLHandler(sPtr);
        break;
    case 11:
        P3SYSCALLHandler(sPtr);
        break;
    default:
        P3TRAPHandler(sPtr); 
        break;
    }
}

void Terminate(support_t *sPtr) {
    SYSCALL(PASSEREN, &SwapTableSemaphore, 0, 0);
    for (int i = 0; i < POOLSIZE; i++)
    {
        if (sPtr->sup_asid == SwapTable[i].sw_asid) {
            SwapTable[i].sw_asid = -1;
            SwapTable[i].sw_pageNo = -1;
            SwapTable[i].sw_pte = NULL;        
        }
    }
    SYSCALL(VERHOGEN, &SwapTableSemaphore, 0, 0);
    SYSCALL(TERMPROCESS, 0, 0, 0);
}

void WritePrinter(support_t *sPtr) {
    state_t *syscallState = &sPtr->sup_exceptState[GENERALEXCEPT];
    //utilizzare systemp call passren e veroghen per semforo P3
    if(syscallState->reg_a2 < 0 || syscallState->reg_a2 > 128)
        SYSCALL(TERMPROCESS, 0, 0, 0);

    int count = 0;
    char     *s      = syscallState->reg_a1;
    memaddr *base    = (memaddr *)(DEV_REG_ADDR(IL_PRINTER, (sPtr->sup_asid-1)));
    memaddr *command = base + 1;
    memaddr  status;

    SYSCALL(PASSEREN, (int)&P3SemaphorePrinter[sPtr->sup_asid-1], 0, 0); /* P(sem_term_mut) */
    while (*s != EOS && count < syscallState->reg_a2) {
        memaddr value = PRINTCHR | (((memaddr)*s) << 8);
        status         = SYSCALL(DOIO, (int)command, (int)value, 0);
        if ((status & 0xFF) != READY) {
            SYSCALL(VERHOGEN, (int)&P3SemaphorePrinter[sPtr->sup_asid-1], 0, 0); /* V(sem_term_mut) */
            syscallState->reg_a0 = -status;
            syscallState->pc_epc += 4;
            LDST(syscallState);
        }
        count++;
        s++;
    }
    SYSCALL(VERHOGEN, (int)&P3SemaphorePrinter[sPtr->sup_asid-1], 0, 0); /* V(sem_term_mut) */
    syscallState->reg_a0 = count;
    syscallState->pc_epc += 4;
    LDST(syscallState);
}

void WriteTerminal(support_t *sPtr){
    state_t *syscallState = &sPtr->sup_exceptState[GENERALEXCEPT];
    //utilizzare systemp call passren e veroghen per semforo P3
    if(syscallState->reg_a2 < 0 || syscallState->reg_a2 > 128)
        SYSCALL(TERMPROCESS, 0, 0, 0);

    int count = 0;
    char     *s      = syscallState->reg_a1;
    memaddr *base    = (memaddr *)(DEV_REG_ADDR(IL_TERMINAL, (sPtr->sup_asid-1)));
    memaddr *command = base + 3;
    memaddr  status;

    SYSCALL(PASSEREN, (int)&P3SemaphoreTerminalTransmitter[sPtr->sup_asid-1], 0, 0); /* P(sem_term_mut) */
    while (*s != EOS && count < syscallState->reg_a2) {
        memaddr value = PRINTCHR | (((memaddr)*s) << 8);
        status         = SYSCALL(DOIO, (int)command, (int)value, 0);
        if ((status & 0xFF) != RECVD) {
            SYSCALL(VERHOGEN, (int)&P3SemaphoreTerminalTransmitter[sPtr->sup_asid-1], 0, 0); /* V(sem_term_mut) */
            syscallState->reg_a0 = -status;
            syscallState->pc_epc += 4;
            LDST(syscallState);
        }
        count++;
        s++;
    } 
    SYSCALL(VERHOGEN, (int)&P3SemaphoreTerminalTransmitter[sPtr->sup_asid-1], 0, 0); /* V(sem_term_mut) */
    syscallState->reg_a0 = count;
    syscallState->pc_epc += 4;
    LDST(syscallState);
}

void ReadTerminal(support_t *sPtr) {
     state_t *syscallState = &sPtr->sup_exceptState[GENERALEXCEPT];
    //utilizzare systemp call passren e veroghen per semforo P3
 

    int count = 0;
    char     *s      = syscallState->reg_a1;
    memaddr *base    = (memaddr *)(DEV_REG_ADDR(IL_TERMINAL, (sPtr->sup_asid-1)));
    memaddr *command = base + 3;
    memaddr  status;
    char carattere;

    SYSCALL(PASSEREN, (int)&P3SemaphoreTerminalReceiver[sPtr->sup_asid-1], 0, 0); /* P(sem_term_mut) */
    while (*s != EOS && count < syscallState->reg_a2 && count < 127) {
        memaddr value = RECEIVECHAR;
        status         = SYSCALL(DOIO, (int)command, (int)value, 0);
        if ((status & 0xFF) != RECVD) {
            SYSCALL(VERHOGEN, (int)&P3SemaphoreTerminalReceiver[sPtr->sup_asid-1], 0, 0); /* V(sem_term_mut) */
            syscallState->reg_a0 = -status;
            syscallState->pc_epc += 4;
            LDST(syscallState);
        }
        carattere = (status >> 8) & 0xFF;
        if (carattere == '\n' || carattere == EOS) {
            *s = EOS; 
            break;
        } else {
            *s = carattere;
        }
        count++;
        s++;
    }
    SYSCALL(VERHOGEN, (int)&P3SemaphoreTerminalReceiver[sPtr->sup_asid-1], 0, 0); /* V(sem_term_mut) */
    syscallState->reg_a0 = count;
    syscallState->pc_epc += 4;
    LDST(syscallState);
}

void P3SYSCALLHandler(support_t *sPtr){
    switch (sPtr->sup_exceptState[GENERALEXCEPT].reg_a0)
    {
    case TERMINATE:
        Terminate(sPtr);
        break;
    
    case WRITEPRINTER:
        WritePrinter(sPtr);
        break;
    
    case WRITETERMINAL:
        WriteTerminal(sPtr);
        break;
    
    case READTERMINAL:
        ReadTerminal(sPtr);
        break;
        
    default:
        P3TRAPHandler(sPtr);
        break;
    }
}

void P3TRAPHandler(support_t *sPtr)
{   
    SYSCALL(PASSEREN, &SwapTableSemaphore, 0, 0);
    for (int i = 0; i < POOLSIZE; i++)
    {
        if (sPtr->sup_asid == SwapTable[i].sw_asid) {
            SwapTable[i].sw_asid = -1;
            SwapTable[i].sw_pageNo = -1;
            SwapTable[i].sw_pte = NULL;        
        }
    }
    SYSCALL(VERHOGEN, &SwapTableSemaphore, 0, 0);
    SYSCALL(TERMPROCESS, 0, 0, 0);
} 

