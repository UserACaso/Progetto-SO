#include "./headers/interrupts.h"


//LDST: RIABILITA LE INTERRUPT.

void PLTHandler(state_t* syscallState) {
    //Acknowledge the PLT interrupt by loading the timer with a new value using setTIMER.
    setTIMER(TIMESLICE);
    int processore = getPRID();
    //Copy the processor state of the current CPU at the time of the exception into the Current Process’s PCB (p_s) of the current CPU.
    pcb_PTR corrente = Current_Process[processore];
    corrente->p_s = *syscallState;
    //Place the Current Process on the Ready Queue; transitioning the Current Process from the “running” state to the “ready” state.
    Current_Process[processore] = NULL;
    insertProcQ(&Ready_Queue, corrente);

    RELEASE_LOCK(&Global_Lock);
    scheduler();
}

void PseudoClockHandler(state_t* syscallState){
    LDIT(PSECOND);
    
    int* indirizzo = &SemaphorePseudo; 
    pcb_PTR blockedProc = NULL;
    while (1)
    {
        if (*indirizzo == 0 && ((blockedProc = removeBlocked((int *)indirizzo)) != NULL)){
            insertProcQ(&Ready_Queue, blockedProc); //inserisco il processo sbloccato nella ready queue
        } else {
            break;
        }
    }
    RELEASE_LOCK(&Global_Lock);
    if (Current_Process[getPRID()] == NULL)
    {
        scheduler();
    }else {//carica lo stato del processore prima delll'interrupt
        LDST(syscallState);
    }
    
}

void DeviceHandler(int IntlineNo, int DevNo, state_t* syscallState){
    devreg_t* devAddrBase = (devreg_t*)0x10000054 + ((IntlineNo - 3) * 0x80) + (DevNo * 0x10);
    unsigned int status;
    int* indirizzo;
    if(IntlineNo != 7)
    {
        status = devAddrBase->dtp.status;
        devAddrBase->dtp.command = ACK;
        switch (IntlineNo)
            {
                case 3: //IL_DISK 
                    indirizzo = &SemaphoreDisk[DevNo];
                    break;
                case 4: //IL_FLASH
                    indirizzo = &SemaphoreFlash[DevNo];
                    break;
                case 5: //IL_ETHERNET
                    indirizzo = &SemaphoreNetwork[DevNo];
                    break;
                case 6: //IL_PRINTER
                    indirizzo = &SemaphorePrinter[DevNo];
                    break;
            }
        pcb_PTR blockedProc = NULL;
        if (*indirizzo == 0 && ((blockedProc = removeBlocked((int *)indirizzo)) != NULL)){ //caso in cui c'è un PCB 
            blockedProc->p_s.reg_a0 = status;
            blockedProc->p_semAdd = NULL;
            insertProcQ(&Ready_Queue, blockedProc); //inserisco il processo sbloccato nella ready queue
        }
    }
    else
    {
        if(devAddrBase->term.transm_status != 0 && devAddrBase->term.transm_status != 1 && devAddrBase->term.transm_status != 3)
        {
            indirizzo = &SemaphoreTerminalTransmitter[DevNo];
            status  = devAddrBase->term.transm_status;
            pcb_PTR blockedProc = NULL;
            if (*indirizzo == 0 && ((blockedProc = removeBlocked((int *)indirizzo)) != NULL)){ //caso in cui c'è un PCB 
                blockedProc->p_s.reg_a0 = status;
                blockedProc->p_semAdd = NULL;
                insertProcQ(&Ready_Queue, blockedProc); //inserisco il processo sbloccato nella ready queue
            }
            devAddrBase->term.transm_command = ACK;
        }
        if(devAddrBase->term.recv_status != 0 && devAddrBase->term.recv_status != 1 && devAddrBase->term.recv_status != 3) {
            indirizzo = &SemaphoreTerminalReceiver[DevNo];
            status = devAddrBase->term.recv_status;
            pcb_PTR blockedProc = NULL;
            if (*indirizzo == 0 && ((blockedProc = removeBlocked((int *)indirizzo)) != NULL)){ //caso in cui c'è un PCB 
                blockedProc->p_s.reg_a0 = status;
                blockedProc->p_semAdd = NULL;
                insertProcQ(&Ready_Queue, blockedProc); //inserisco il processo sbloccato nella ready queue
            }
            devAddrBase->term.recv_command = ACK;
        }
    }
    RELEASE_LOCK(&Global_Lock);
    if (Current_Process[getPRID()] == NULL)
    {
        scheduler();
    }else {//carica lo stato del processore prima delll'interrupt
        LDST(syscallState);
    }

}

int DevNOGet(unsigned int Linea) {
    unsigned int temp = 0;
    for(int i = 0; i < 8; i++){ //ritorna il device della linea
        if(*((memaddr *)(0x10000040) + (Linea-3)*0x4 ) & (DEV0ON << i)){
            return i;
        }
    }
    PANIC();
}
//NOTA: "syscallState" è lo stato del processore un attimo prima dell'arrivo dell'interrupt
void InterruptHandler(state_t* syscallState, unsigned int excode){
    ACQUIRE_LOCK(&Global_Lock);
    unsigned int cause = getCAUSE();
    for (int i = 0; i < 8; ++i) {
        unsigned int temp = 0;
        if ((temp = CAUSE_IP_GET(cause, i))) {
            switch (i) {
                case 1:
                    PLTHandler(syscallState);
                    break;
                case 2:
                    PseudoClockHandler(syscallState);
                    break;
                default:
                    DeviceHandler(i, DevNOGet(i), syscallState);
                    break;
            }
            break;
        }
    }

    //controllo che vi siano degli interrupt a priorità più alta (partendo dal basso)
    //se arriva un interrupt quando tutti i processori sono occupati, (e poi ne arriva un altra ancora)
    //Controllare come fare la bitmap per i vari device (per i device intlineno > 3, lo si fa)
    //gestione interrupt se tutti i processi sono gia occupati quindi con tLB messa a 0 
    //check di quale processore e' settato con la tbl a 1
    //e far gestire a lui (confronto con 0...0 e tbl)

}