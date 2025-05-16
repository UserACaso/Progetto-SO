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

/* CreateProcess(): la funzione permette ad un processo di creare un figlio. inizia usando allocPCB.
se il processo non può essere creato perché non ci sono più PCB disponibili, un error code -1 viene messo nell'a0 del chiamante.
altrimenti il processo viene inizializzato: vengono copiate le informazioni del processo padre, vengono inserite nella ready queue e viene collegato
come figlio al processo corrente e gli viene restituito il suo PID
*/

void Terminator(pcb_PTR p) //Pass Up or Die
{
    //all PCB A PCB is either the Current Process (“running”), 
    //sitting on the Ready Queue (“ready”), 
    //blocked waiting for device (“blocked”), 
    //or blocked waiting for non-device (“blocked”).
    //caso in cui il processo figlio ha 2 figli e in cui uno dei due figli ha
        
    //orfanare il padre dal processo, se esiste un processo padre
    if(p->p_parent != NULL) {
        outChild(p);
    }

    for (int i = 0; i < NCPU; i++)
    {
        if(Current_Process[i] == p)
        {
            Current_Process[i] = NULL;
            break;
        }
    }

    while (!emptyChild(p)) //lista non vuota, ossia il processo "p" ha dei figli
    {
        pcb_PTR figlio = removeChild(p); //rimuove solo il primo figlio
        Terminator(figlio); //chiamata ricorsiva
    }

    Process_Count--; //Process_Count reajusted
    outBlocked(p);
    freePcb(p);
}



void CreateProcess(state_t* syscallState, pcb_PTR corrente){
    //Inizializzazione variabili inizio e fine tempo per syscall 
    cpu_t current_time_inizio, current_time_fine; 
    STCK(current_time_inizio);
      
    pcb_PTR newP = allocPcb();

    if (newP == NULL) { //se allocazione pcb fallisce
        syscallState->reg_a0 = -1; // segnalo errore allocazione PCB        
    } else {
        state_t* newState = (state_t*) syscallState->reg_a1; //ottengo lo stato del nuovo processo
        support_t* newSupport = (support_t*) syscallState->reg_a3; //ottengo il puntatore alla struttura di supporto (se non esiste è NULL)
        newP->p_s = *newState;
        newP->p_supportStruct = newSupport;

        insertProcQ(&Ready_Queue, newP);     // Inserisci nella ready queue
        insertChild(corrente, newP);         // Aggiungi come figlio del processo corrente
        Process_Count++;

        // p_time e p_semAdd sono impostati a 0 e a NULL dalla allocPcb

        syscallState->reg_a0 = newP->p_pid;  // Restituisci il pid del nuovo processo in a0

    }
    syscallState->pc_epc += 4; // program counter + 4
    STCK(current_time_fine);
    corrente->p_time += current_time_fine - current_time_inizio; //aggiorno il tempo di vita del processo
    RELEASE_LOCK(&Global_Lock);
    LDST(syscallState); //ricarico lo stato precedente della cpu
}


/* TerminateProcess(): La funzione ci permette di terminare un processo, recursivamente eliminando tutta la progenia del processo con esso.

testa e finisci commenti
*/


void TerminateProcess(state_t* syscallState, pcb_PTR corrente) {
     // Get the PID parameter from a1 register
     cpu_t current_time_inizio, current_time_fine; 
     STCK(current_time_inizio);

     int pid = syscallState->reg_a1;
    
     if (pid == 0) {
         // Terminate the calling process
         Terminator(corrente);
     } else {
         // Find the process with the specified PID and terminate it
         pcb_PTR target = NULL;
         
         // Search in Ready Queue
         struct list_head* iterator;
         list_for_each(iterator, &Ready_Queue) {
             pcb_PTR temp = container_of(iterator, pcb_t, p_list);
             if (temp->p_pid == pid) {
                 target = temp;
                 // Remove from ready queue
                 outProcQ(&Ready_Queue, target);
                 break;
             }
         }
         
         // If not found in Ready Queue, search in Current Processes
         if (target == NULL) {
             for (int i = 0; i < NCPU; i++) {
                 if (Current_Process[i] != NULL && Current_Process[i]->p_pid == pid) {
                     target = Current_Process[i];
                     Current_Process[i] = NULL;
                     break;
                 }
             }
         }
         
         // If not found in Current Processes, check if it's blocked on a semaphore
         if (target == NULL) {
             target = outBlockedPid(pid);
         }
         
         // If the process was found, terminate it
         if (target != NULL) {
             Terminator(target);
         }
     }
     
     // Release lock
     STCK(current_time_fine);
     corrente->p_time += current_time_fine - current_time_inizio; 
     RELEASE_LOCK(&Global_Lock);
     
     // Call the scheduler
     scheduler();
};




//i processi bloccati su dei semafori che successivamente vengono sbloccati vanno messi sulla process ready queue (forse)

/* Passeren(): castiamo reg_a1 (che contiene l'indirizzo del semaforo) a un puntatore memadrr (unsigned int). Controlliamo all'interno della coda
               del semaforo se esistono delle .V(): 
                -se è possibile eseguirla (ovviamente quando il valore del semaforo è 1!), allora la si esegue e si sblocca il processo. Il processo è pronto, va messo sulla ready queue. 
                -se non è possibile eseguirla, controlliamo se il valore del semaforo è 1, ed eventualmente decrementiamo;
                -altrimenti inseriamo il processo nella lista dei processi bloccati del semaforo. ATTENZIONE: se non c'è più posto nella lista,
                    bisogna segnalare all'utente che non è possibile bloccare il processo (PANIC()).
*/


void Passeren(state_t* syscallState, pcb_PTR corrente){
    //Inizializzazione variabili inizio e fine tempo per syscall 
    cpu_t current_time_inizio, current_time_fine; 
    STCK(current_time_inizio);

    memaddr* semaddr = (memaddr *)syscallState->reg_a1; //semaddr contiene l'indirizzo del semaforo
    pcb_PTR blockedProc = NULL;

    if (*semaddr == 1 && ((blockedProc = removeBlocked((int *)semaddr)) != NULL)) //Se il semaforo vale 1, allora è possibile fare una P: controllo se il semaforo ha un processo bloccato e lo sblocco
    { 
        syscallState->pc_epc += 4; // salto di una word per evitare che la syscall vada in loop
        STCK(current_time_fine);
        corrente->p_time += current_time_fine - current_time_inizio; //aggiorno il tempo di vita del processo
        insertProcQ(&Ready_Queue, blockedProc); //inserisco il processo sbloccato nella ready queue
        RELEASE_LOCK(&Global_Lock); 
        LDST(syscallState); //ricarico lo stato precedente della cpu
    } else if (*semaddr == 1) // decrementa il semaforo
    { 
        *semaddr = 0; 
        syscallState->pc_epc += 4; 
        STCK(current_time_fine);
        corrente->p_time += current_time_fine - current_time_inizio; //aggiorno il tempo di vita del processo
        RELEASE_LOCK(&Global_Lock); 
        LDST(syscallState); //ricarico lo stato precedente della cpu
    } else { //il processo si blocca sul semaforo e viene chiamato lo scheduler
        syscallState->pc_epc += 4;
        corrente->p_s = *syscallState;
        corrente->p_semAdd = semaddr;
        int temp = insertBlocked((int*)semaddr, corrente);
        if (temp == 1){ //se non è possibile creare nuovi processi
            PANIC();
        }

        //incremento cpu time del processo corrente
        STCK(current_time_fine); 
        corrente->p_time += current_time_fine - current_time_inizio;
        Current_Process[getPRID()] = NULL;
        RELEASE_LOCK(&Global_Lock); //rilascio la variabile globale
        scheduler(); //richiamo allo scheduler

    }
    
}


void Verhogen(state_t* syscallState, pcb_PTR corrente){
    //Inizializzazione variabili inizio e fine tempo per syscall 
    cpu_t current_time_inizio, current_time_fine;
    STCK(current_time_inizio);

    memaddr* semaddr = (memaddr *)syscallState->reg_a1;
    pcb_PTR blockedProc = NULL;

    if (*semaddr == 0 && ((blockedProc = removeBlocked((int *)semaddr)) != NULL))
    { //controlla se nella coda del semaforo se esiste una P: nel caso la blocca e si blocca anche questo processo
        syscallState->pc_epc += 4;  // salto di una word per evitare che la syscall vada in loop
        STCK(current_time_fine); 
        corrente->p_time += current_time_fine - current_time_inizio; //aggiorno il tempo di vita del processo
        insertProcQ(&Ready_Queue, blockedProc); //inserisco il processo sbloccato nella ready queue
        RELEASE_LOCK(&Global_Lock); 
        LDST(syscallState); //ricarico lo stato precedente della cpu
    } else if (*semaddr == 0)
    {// incremento il semaforo
        *semaddr = 1;
        syscallState->pc_epc += 4;
        STCK(current_time_fine);
        corrente->p_time += current_time_fine - current_time_inizio;
        RELEASE_LOCK(&Global_Lock); 
        LDST(syscallState); //ricarico lo stato precedente della cpu
    } else { //il processo viene bloccato sul semaforo
        syscallState->pc_epc += 4;
        corrente->p_s = *syscallState;
        corrente->p_semAdd = semaddr;
        int temp = insertBlocked((int*)semaddr, corrente);
        if (temp == 1) { //se non è possibile creare nuovi processi
            PANIC();
        }

        //incremento cpu time del processo corrente
        STCK(current_time_fine);
        corrente->p_time += current_time_fine - current_time_inizio;
        Current_Process[getPRID()] = NULL;
        RELEASE_LOCK(&Global_Lock); //rilascio la variabile globale
        scheduler(); //richiamo allo scheduler
    }
    
}
// la funzione GetCPUTime() ci permette di restituire il tempo d'esecuzione del processo che l’ha chiamata
void GetCPUTime(state_t* syscallState, pcb_PTR corrente) { //non va?
    cpu_t current_time_inizio, current_time_fine;
    STCK(current_time_inizio);
    
    syscallState->reg_a0 = corrente->p_time; //restituisco il tempo d'esecuzione nell'a0 del chiamante
    
    syscallState->pc_epc += 4; // program counter + 4
    STCK(current_time_fine);
    corrente->p_time += current_time_fine - current_time_inizio;
    RELEASE_LOCK(&Global_Lock); 
    LDST(syscallState); //ricarico lo stato precedente della cpu
}

void WaitForClock(state_t* syscallState, pcb_PTR corrente) {
    cpu_t current_time_inizio, current_time_fine;
    STCK(current_time_inizio);
    memaddr* semaddr = &SemaphorePseudo; 
    syscallState->pc_epc += 4; // salto di una word per evitare che la syscall vada in loop
    corrente->p_s = *syscallState;
    corrente->p_semAdd = semaddr;
    int temp = insertBlocked((int*)semaddr, corrente); //blocco il processo corrente sul semaforo dello Pseudo clock
    if (temp == 1) //se non è possibile creare nuovi processi
    { /* gestione errore del semaforo, magari gestire con una trap */
        PANIC();
    }
    
    STCK(current_time_fine);
    corrente->p_time += current_time_fine - current_time_inizio; //incremento cpu time del processo corrente
    Current_Process[getPRID()] = NULL;
    RELEASE_LOCK(&Global_Lock); 
    scheduler();
}

// la funzione GetSupportData() ci permette di restituire un puntatore alla support struct del processo current.
void GetSupportData(state_t* syscallState, pcb_PTR corrente) {
    cpu_t current_time_inizio, current_time_fine;
    STCK(current_time_inizio);
    support_t* support = corrente->p_supportStruct;
    
    syscallState->reg_a0 = (memaddr)support; //se non esiste NULL sennò puntatore valido
    
    syscallState->pc_epc += 4; // program counter + 4
    STCK(current_time_fine);
    corrente->p_time += current_time_fine - current_time_inizio; //aggiorno il tempo di vita del processo
    RELEASE_LOCK(&Global_Lock); 
    LDST(syscallState); //ricarico lo stato precedente della cpu
}

//la funzione GetProcessID ci permette di restituire l'identificatore del chiamante se il parent è 0, altrimenti restituisce l'id del genitore
void GetProcessID(state_t* syscallState, pcb_PTR corrente) {
    cpu_t current_time_inizio, current_time_fine;
    STCK(current_time_inizio);

    if (syscallState->reg_a1 != 0) {
        if (corrente->p_parent != NULL) { // Se il processo corrente ha un padre
            syscallState->reg_a0 = corrente->p_parent->p_pid; // Restituisco il pid del padre
        } else {
            syscallState->reg_a0 = 0; // Se non esiste un padre, restituisco 0
        }
    } else {
        syscallState->reg_a0 = corrente->p_pid;
    }
    
    syscallState->pc_epc += 4; // program counter + 4
    STCK(current_time_fine);
    corrente->p_time += current_time_fine - current_time_inizio; //aggiorno il tempo di vita del processo
    RELEASE_LOCK(&Global_Lock); 
    LDST(syscallState); //ricarico lo stato precedente della cpu
}


void DoIo(state_t* syscallState, pcb_PTR corrente) {
    cpu_t current_time_inizio, current_time_fine;
    STCK(current_time_inizio);
    int commandAddr = syscallState->reg_a1;
    int commandValue = syscallState->reg_a2;
    unsigned int base = commandAddr;
    int* semaddr = NULL;
    unsigned int pos = (commandAddr - TERM0ADDR)%0x10; //ricavo l'indirizzo del device register
    
    if (commandAddr >= TERM0ADDR)
    {
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
    //devreg_t* devAddrBase = 0x10000054 + ((linea - 3) * 0x80) + (device * 0x10);
    devreg_t* devAddrBase = (devreg_t*)(0x10000054 + ((linea - 3) * 0x80) + (device * 0x10));
    //linea 3 + 4
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
        default: //per sicurezza, se mai accadesse un errore
            PANIC();
    }

    //Verificare anche nel caso 2 processi cercano di fermarsi sullo stesso device 
    syscallState->pc_epc += 4;
    corrente->p_s = *syscallState;
    corrente->p_semAdd = semaddr;

    int temp = insertBlocked((int*)semaddr, corrente);
    if (temp == 1) //se non è possibile creare nuovi processi
        PANIC();

    if(linea == 7)
    {
        if(pos == 0xC)
        {
            STCK(current_time_fine); 
            corrente->p_time += current_time_fine - current_time_inizio;
            devAddrBase->term.transm_command = commandValue; 
            Current_Process[getPRID()] = NULL;  
            RELEASE_LOCK(&Global_Lock);     
            scheduler();
        }
        else
        {
            STCK(current_time_fine); 
            corrente->p_time += current_time_fine - current_time_inizio;
            devAddrBase->term.recv_command = commandValue;   
            Current_Process[getPRID()] = NULL;  
            RELEASE_LOCK(&Global_Lock);     
            scheduler();
        }
    }
    else
    {
        STCK(current_time_fine); 
        corrente->p_time += current_time_fine - current_time_inizio;
        devAddrBase->dtp.command = commandValue;
        Current_Process[getPRID()] = NULL;  
        RELEASE_LOCK(&Global_Lock);     
        scheduler();
    }
}




void SYSCALLHandler(state_t* syscallState, unsigned int cpuid){
    ACQUIRE_LOCK(&Global_Lock);
    pcb_PTR corrente = Current_Process[cpuid];
    if((corrente->p_s.status & MSTATUS_MPP_MASK) != MSTATUS_MPP_M)
    {   
        RELEASE_LOCK(&Global_Lock);
        TRAPHandler(syscallState, cpuid);    
        return;
    }

    switch(syscallState->reg_a0) //only in kernel mode (if in user mode -> program trap)
    {
    case -1: //Create Process (SYS1)
        CreateProcess(syscallState, corrente);
        break;

    case -2: //Terminate Process (SYS2)
        TerminateProcess(syscallState, corrente);
        break;
    
    case -3: //Passeren (P)
        Passeren(syscallState, corrente);
        break;

    case -4: // Verhogen (V)
        Verhogen(syscallState, corrente);
        break;

    case -5: //DoIO (NSYS5)
        DoIo(syscallState, corrente);
        break;
    case -6: //GetCPUTime (NSYS6)
        GetCPUTime(syscallState, corrente);
        break;
    
    case -7: //WaitForClock (NSYS7)
        WaitForClock(syscallState, corrente);
        break;

    case -8: //GetSupportData (NSYS8)
        GetSupportData(syscallState, corrente);
        break;
    
    case -9: //GetProcessID (NSYS9)
        GetProcessID(syscallState, corrente);
        break;
        
    default:
        RELEASE_LOCK(&Global_Lock);
        TRAPHandler(syscallState, cpuid);
    }
}

void TLBHandler(state_t* syscallState, unsigned int cpuid){
    ACQUIRE_LOCK(&Global_Lock);
    pcb_PTR corrente = Current_Process[cpuid];
    if (Current_Process[cpuid]->p_supportStruct == NULL) 
    {
        //orfanare il padre dal processo, se esiste un processo padre
        Terminator(corrente);   
        RELEASE_LOCK(&Global_Lock);
        scheduler();
        
    }
    else //if(corrente->p_supportStruct != NULL) //passed up
    {
        corrente->p_supportStruct->sup_exceptState[PGFAULTEXCEPT] = *syscallState;
        context_t temp = corrente->p_supportStruct->sup_exceptContext[PGFAULTEXCEPT];
        RELEASE_LOCK(&Global_Lock);
        LDCXT(temp.stackPtr, temp.status, temp.pc);
    }
}

void TRAPHandler(state_t* syscallState, unsigned int cpuid) {
    ACQUIRE_LOCK(&Global_Lock);
    pcb_PTR corrente = Current_Process[cpuid];
    if (Current_Process[cpuid]->p_supportStruct == NULL) 
    { 
        Terminator(corrente);   
        RELEASE_LOCK(&Global_Lock);
        scheduler();
        //Process_Count reajusted
        //ricorsione!
        //all PCB A PCB is either the Current Process (“running”), 
        //sitting on the Ready Queue (“ready”), 
        //blocked waiting for device (“blocked”), 
        //or blocked waiting for non-device (“blocked”).
    }
    else //passed up
    {
        corrente->p_supportStruct->sup_exceptState[GENERALEXCEPT] = *syscallState;
        context_t temp = corrente->p_supportStruct->sup_exceptContext[GENERALEXCEPT];
        RELEASE_LOCK(&Global_Lock);
        LDCXT(temp.stackPtr, temp.status, temp.pc);
    }
    
}
