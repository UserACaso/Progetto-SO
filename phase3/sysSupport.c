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
        P3TRAPHandler(); 
        break;
    }
}

void Terminate() {
    SYSCALL(TERMPROCESS, 0, 0, 0);
}

void WritePrinter(support_t *sPtr) {
    state_t *syscallState = &sPtr->sup_exceptState[GENERALEXCEPT];
    syscallState
}

void WriteTerminal(support_t *sPtr){
    SYSCALL();
}

void ReadTerminal(support_t *sPtr) {
    SYSCALL();
}

void P3SYSCALLHandler(support_t *sPtr){
    switch (sPtr->sup_exceptState[GENERALEXCEPT].reg_a0)
    {
    case TERMINATE:
        Terminate();
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
        //
        break;
    }
}
