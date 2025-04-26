#include "./headers/exceptions.h"
#define TERM0ADDR 0x10000254

/*
    Quando avviene un'eccezione, all'interno del BIOS Data Page vengono salvate le informazioni 
    correnti degli stati di TUTTI i processori.
    GET_EXCEPTION_STATE_PTR() calcola l'indirizzo del processore il cui stato è stato salvato nella 
    BIOS DATA PAGE.
    Quindi con il comando GET_EXCEPTION_STATE_PTR(getPRID()) si ottiene un puntantore state_t che
    contiene le informazioni dei registri del processore attuale.

*/

//i processi bloccati su dei semafori che successivamente vengono sbloccati vanno messi sulla process ready queue (forse)

/* Passeren(): castiamo reg_a1 (che contiene l'indirizzo del semaforo) a un puntatore memadrr (unsigned int). Controlliamo all'interno della coda
               del semaforo se esistono delle .V(): 
                -se è possibile eseguirla (ovviamente quando il valore del semaforo è 1!), allora la si esegue e si sblocca il processo. Il processo è pronto, va messo sulla ready queue. 
                -se non è possibile eseguirla, controlliamo se il valore del semaforo è 1, ed eventualmente decrementiamo;
                    -altrimenti inseriamo il processo nella lista dei processi bloccati del semaforo. ATTENZIONE: se non c'è più posto nella lista,
                     bisogna segnalare all'utente che non è possibile bloccare il processo (PANIC()).
*/

void Passeren(state_t* syscallState, pcb_PTR corrente){
    cpu_t current_time_inizio, current_time_fine;
    STCK(current_time_inizio);
    memaddr* semaddr = (memaddr *)syscallState->reg_a1; //semaddr contiene l'indirizzo del semaforo
    pcb_PTR blockedProc = NULL;
    if (*semaddr == 1 && ((blockedProc = removeBlocked((int *)semaddr)) != NULL)){ //controlla se nella coda del semaforo se esiste una V: nel caso la sblocca e si sblocca anche questo processo
        syscallState->pc_epc += 4;
        STCK(current_time_fine);
        corrente->p_time += current_time_fine - current_time_inizio;
        insertProcQ(&Ready_Queue, blockedProc); //inserisco il processo sbloccato nella ready queue
        RELEASE_LOCK(&Global_Lock); 
        LDST(syscallState);
    } else if (*semaddr == 1){ // control is returned to the Current Process of the current processor
        *semaddr = 0; 
        syscallState->pc_epc += 4; 
        STCK(current_time_fine);
        corrente->p_time += current_time_fine - current_time_inizio;
        RELEASE_LOCK(&Global_Lock); 
        LDST(syscallState);
    } else { //process is blocked on the ASL and the Scheduler is called
        syscallState->pc_epc += 4;
        corrente->p_s = *syscallState;
        corrente->p_semAdd = semaddr;
        int temp = insertBlocked((int*)semaddr, corrente);
        if (temp == 1) //se non è possibile creare nuovi processi
        { /* gestione errore del semaforo, magari gestire con una trap */
            PANIC();
        }
        
        STCK(current_time_fine); //incremento cpu time del processo corrente
        corrente->p_time += current_time_fine - current_time_inizio;
        RELEASE_LOCK(&Global_Lock); //here
        scheduler();

    }
    
}

void Verhogen(state_t* syscallState, pcb_PTR corrente){
    cpu_t current_time_inizio, current_time_fine;
    STCK(current_time_inizio);
    memaddr* semaddr = (memaddr *)syscallState->reg_a1;
    pcb_PTR blockedProc = NULL;
    if (*semaddr == 0 && ((blockedProc = removeBlocked((int *)semaddr)) != NULL)){ //controlla se nella coda del semaforo se esiste una P: nel caso la blocca e si blocca anche questo processo
        syscallState->pc_epc += 4;
        STCK(current_time_fine);
        corrente->p_time += current_time_fine - current_time_inizio;
        insertProcQ(&Ready_Queue, blockedProc); //inserisco il processo sbloccato nella ready queue
        RELEASE_LOCK(&Global_Lock); 
        LDST(syscallState);
    } else if (*semaddr == 0){// control is returned to the Current Process of the current processor
        *semaddr = 1;
        syscallState->pc_epc += 4;
        STCK(current_time_fine);
        corrente->p_time += current_time_fine - current_time_inizio;
        RELEASE_LOCK(&Global_Lock); 
        LDST(syscallState);
    } else { //process is blocked on the ASL and the Scheduler is called
        syscallState->pc_epc += 4;
        corrente->p_s = *syscallState;
        corrente->p_semAdd = semaddr;
        int temp = insertBlocked((int*)semaddr, corrente);
        if (temp == 1) //se non è possibile creare nuovi processi
        { /* gestione errore del semaforo */
            PANIC();
        }
        
        STCK(current_time_fine); //incremento cpu time del processo corrente
        corrente->p_time += current_time_fine - current_time_inizio;
        RELEASE_LOCK(&Global_Lock); //here
        
        scheduler();
    }
    
}

void WaitForClock(state_t* syscallState, pcb_PTR corrente) {
    cpu_t current_time_inizio, current_time_fine;
    STCK(current_time_inizio);
    memaddr* semaddr = &SemaphorePseudo; 
    syscallState->pc_epc += 4;
    corrente->p_s = *syscallState;
    corrente->p_semAdd = semaddr;
    int temp = insertBlocked((int*)semaddr, corrente);
    if (temp == 1) //se non è possibile creare nuovi processi
    { /* gestione errore del semaforo, magari gestire con una trap */
        PANIC();
    }
    
    STCK(current_time_fine); //incremento cpu time del processo corrente
    corrente->p_time += current_time_fine - current_time_inizio;
    RELEASE_LOCK(&Global_Lock); 
    scheduler();
    
}

void DoIo(state_t* syscallState, pcb_PTR corrente) {
    cpu_t current_time_inizio, current_time_fine;
    STCK(current_time_inizio);
    int commandAddr = syscallState->reg_a1;
    int commandValue = syscallState->reg_a2;
    unsigned int base = commandAddr;
    int* semaddr = NULL;
    unsigned int pos = 0;
    
    if (commandAddr >= TERM0ADDR)
    {
        pos = commandAddr & 0xF;
        switch (pos)
        {
            case 0xC:
                base -= 0xC; 
                break;
            case 0x4:
                base -= 0x4;
                break;
        }   
    } else {
        base -= 0x4;
    }

    //diff = [commandAddr - 0x1000.0054] = numeri di bit corrispondenti fino all'indirizzo che ci viene dato 
    //per calcolare la linea si utilizza la divisione intera diff / 128 + 3 (partiamo dalla linea 3).
    //per calcolare il device il resto lo si divide per 16 sempre utilizzando la divisione intera
    unsigned int diff = base - 0x10000054;
    int linea =  diff/0x80 + 3; //abbiamo convertito il numer0 128 in esadecimale
    int device = (diff%(0x80))/(0x10); //abbiamo convertito i numeri in esadecimale
    
    devreg_t* devAddrBase = 0x10000054 + ((linea - 3) * 0x80) + (device * 0x10);
    
    switch (linea)
    {
        case 3: //IL_DISK 
            semaddr = &SemaphoreDisk[device];
            break;
        case 4: //IL_FLASH
            semaddr = &SemaphoreFlash[device];
            break;
        case 5: //IL_ETHERNET
            semaddr = &SemaphoreNetwork[device];
            break;
        case 6: //IL_PRINTER 
            semaddr = &SemaphorePrinter[device];
            break;
        case 7:
            if(pos == 0xC){
                semaddr = &SemaphoreTerminalTransmitter[device];
            } else{
                semaddr = &SemaphoreTerminalReceiver[device];
            } 
            break;
        default: //per sicurezza, se mai accadesse un errore inesistente
            PANIC();
    }

//Verificare anche nel caso 2 processi cercano di fermarsi sullo stesso device 
    corrente->p_s = *syscallState;
    corrente->p_semAdd = semaddr;
    int temp = insertBlocked((int*)semaddr, corrente);
    if (temp == 1) //se non è possibile creare nuovi processi
        PANIC();

    syscallState->pc_epc += 4;

    if(linea == 7)
    {
        if(pos == 0xC)
        {
            STCK(current_time_fine); 
            corrente->p_time += current_time_fine - current_time_inizio;
            devAddrBase->term.transm_command = commandValue;   
            RELEASE_LOCK(&Global_Lock);     
            scheduler();
        }
        else
        {
            STCK(current_time_fine); 
            corrente->p_time += current_time_fine - current_time_inizio;
            devAddrBase->term.recv_command = commandValue;   
            RELEASE_LOCK(&Global_Lock);     
            scheduler();
        }
    }
    else
    {
        STCK(current_time_fine); 
        corrente->p_time += current_time_fine - current_time_inizio;
        devAddrBase->dtp.command = commandValue;
        RELEASE_LOCK(&Global_Lock);     
        scheduler();
    }
}

void SYSCALLHandler(state_t* syscallState, unsigned int cpuid){
    ACQUIRE_LOCK(&Global_Lock);
    pcb_PTR corrente = Current_Process[cpuid];
    if(corrente->p_s.status & MSTATUS_MPP_MASK != MSTATUS_MPP_M)
    {   
        TRAPHandler(syscallState, cpuid);    
        return;
    }

    switch(syscallState->reg_a0) //only in kernel mode (if in user mode -> program trap)
    {
    case -1: //Create Process (SYS1)
        
        break;

    case -2: //Terminate Process (SYS2)
        
        break;
    
    case -3: //Passeren (P)
        Passeren(syscallState, corrente);
        break;

    case -4: // Verhogen (V)
        Verhogen(syscallState, corrente);
        break;

    case -5: //DoIO (NSYS5) *
        DoIo(syscallState, corrente);
        break;
    case -6: //GetCPUTime (NSYS6)
        //ritorna p_supportStruct
        break;
    
    case -7: //WaitForClock (NSYS7)
        WaitForClock(syscallState, corrente);
        break;

    case -8: //GetSupportData (NSYS8)
        //ritorna
        break;
    
    case -9: //GetProcessID (NSYS9)
        //ritorna
        break;
    
    default: // Program Trap exception
        TRAPHandler(syscallState, cpuid);
        break;
    }
}

void TLBHandler(){
    
}
void TRAPHandler(state_t* syscallState, unsigned int cpuid) {
    if (Current_Process[cpuid]->p_supportStruct == NULL) 
    {
        ACQUIRE_LOCK(&Global_Lock);
        //Process_Count reajusted
        
        //all PCB A PCB is either the Current Process (“running”), 
        //sitting on the Ready Queue (“ready”), 
        //blocked waiting for device (“blocked”), 
        //or blocked waiting for non-device (“blocked”).
        //caso in cui il processo figlio ha 2 figli e in cui uno dei due figli ha
        RELEASE_LOCK(&Global_Lock);
    }
    
}