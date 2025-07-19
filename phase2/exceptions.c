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


/*
    Terminator è una funzione ricorsiva che termina un processo e tutti i suoi discendenti.
    Rimuove il processo dalla lista dei figli del padre, termina ricorsivamente tutti i figli,
    lo rimuove dai semafori su cui e' bloccato e dealloca il PCB.
*/

void Terminator(pcb_PTR p) // Pass Up or Die
{
    // Rimuovere il processo dalla lista dei figli del padre, se esiste
    if(p->p_parent != NULL) {
        outChild(p);
    }

    // Impostare a NULL il Current_Process nella cpu dove il processo stava venendo eseguito
    for (int i = 0; i < NCPU; i++)
    {
        if(Current_Process[i] == p)
        {
            Current_Process[i] = NULL;
            break;
        }
    }

    while (!emptyChild(p))
    {
        pcb_PTR figlio = removeChild(p); // Rimuove il primo figlio
        Terminator(figlio); // Chiamata ricorsiva sul figlio
    }

    Process_Count--; // Decrementare il conteggio dei processi
    outBlocked(p); // Rimuove il processo dai semafori su cui e' bloccato, se presente
    freePcb(p);
}


/*
    CreateProcess(): la funzione permette ad un processo di creare un figlio. Inizia usando allocPCB.
    Se il processo non può essere creato perché non ci sono più PCB disponibili, viene messo error code -1 nel registro ao 0 del chiamante.
    Altrimenti il nuovo processo viene inizializzato: vengono copiate le informazioni del processo padre, viene inserite nella ready queue,
    viene collegato come figlio al processo corrente, e viene restituito il suo pid.
*/

void CreateProcess(state_t* syscallState, pcb_PTR corrente){

    pcb_PTR newP = allocPcb(); //allocazione pcb

    if (newP == NULL) { //se l'allocazione fallisce
        syscallState->reg_a0 = -1; // segnalo errore allocazione PCB
    } else {
        state_t* newState = (state_t*) syscallState->reg_a1; //ottengo lo stato del nuovo processo
        support_t* newSupport = (support_t*) syscallState->reg_a3; //ottengo il puntatore alla struttura di supporto (se non esiste è NULL)
        newP->p_s = *newState;
        newP->p_supportStruct = newSupport;
        
        if (newSupport != NULL) {
            klog_print("CreateProcess: Support struct assigned");
        } else {
            klog_print("CreateProcess: Support struct is NULL");
        }

        insertProcQ(&Ready_Queue, newP);     // lo inserisco nella ready queue
        insertChild(corrente, newP);         // lo aggiungo come figlio del processo corrente
        Process_Count++;                     // aggiorno il contatore
        // p_time e p_semAdd sono impostati a 0 e a NULL dalla allocPcb

        syscallState->reg_a0 = newP->p_pid;  // restituisco il pid del nuovo processo in a0

    }
    syscallState->pc_epc += 4; //eseguo salto di una word per evitare loop
    RELEASE_LOCK(&Global_Lock);
    LDST(syscallState); //ricarico lo stato precedente della cpu
}


/* 
    TerminateProcess(): La funzione permette di terminare un processo specifico e, ricorsivamente, anche tutta la sua progenie con esso stesso.
    Va innanzitutto controllato se il processo da terminare è quello corrente ed, in tal caso, il pid sarà 0 e dobbiamo indicarlo come target.
    Se il pid è diverso da 0, il processo da terminare viene cercato nella ready queue, tra i processi correnti e tra i quelli bloccati.
    Se il processo viene trovato, va passato alla funzione Terminator() e viene chiamato lo scheduler.
*/

void TerminateProcess(state_t* syscallState, pcb_PTR corrente) {

    int pid = syscallState->reg_a1; // pid del processo da terminare
    pcb_PTR target = NULL;

    if (pid == 0) { //se pid == 0, allora termino il processo corrente
        target = corrente;
        Current_Process[getPRID()] = NULL;
    } else {
        // Ricerco nella ready queue
        struct list_head *iter;
        list_for_each(iter, &Ready_Queue) { //itero la ready queue
            pcb_PTR temp = container_of(iter, pcb_t, p_list);
            if (temp->p_pid == pid) {
                target = temp;
                outProcQ(&Ready_Queue, target); // rimuovo dalla ready queue
                break;
            }
        }
        // Ricerco nei processi correnti
        if (target == NULL) {
            for (int i = 0; i < NCPU; i++) {
                if (Current_Process[i] != NULL && Current_Process[i]->p_pid == pid) {
                    target = Current_Process[i]; //se trovato, lo segnalo come target
                    Current_Process[i] = NULL;
                    break;
                }
            }
        }
        // Ricerco nei bloccati dei semafori
        if (target == NULL) {
            target = outBlockedPid(pid); // dealloco dall'ASL
        }
    }
    if (target != NULL) {
        Terminator(target); // termino ricorsivamente il processo e i suoi figli
    }
    RELEASE_LOCK(&Global_Lock);
    scheduler(); // chiamata scheduler
}

/* 
    Passeren(): la funzione ci permette di eseguire una operazione di .P() sul semaforo binario passato alla funzione attraverso il registro a0.
    Si controlla se sia possibile rimuovere il primo processo bloccato sul semaforo (ovviamente se il valore del semaforo è 1): se è possibile allora il processo viene sbloccato e inserito 
    nella coda dei processi "ready" (la ReadyQueue); se il valore del semaforo è 1 e sul semaforo non vi sono processi bloccati, si decrementa il valore di quest'ultimo. Altrimenti il processo 
    deve rimanere bloccato sul semaforo
*/

void Passeren(state_t* syscallState, pcb_PTR corrente){

    memaddr* semaddr = (memaddr *)syscallState->reg_a1; 
    pcb_PTR blockedProc = NULL;

    if (*semaddr == 1 && ((blockedProc = removeBlocked((int *)semaddr)) != NULL))
    {
        syscallState->pc_epc += 4; // salto di una word per evitare che la syscall vada in loop
        insertProcQ(&Ready_Queue, blockedProc); //inserisco il processo sbloccato nella ready queue
        RELEASE_LOCK(&Global_Lock); 
        LDST(syscallState); //ricarico lo stato precedente della cpu
    } else if (*semaddr == 1) // decrementa il semaforo
    {
        *semaddr = 0; 
        syscallState->pc_epc += 4; 
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
        cpu_t now;
        STCK(now);
        Current_Process[getPRID()]->p_time += (now - Timestamp[getPRID()]); //incremento cpu time del processo corrente
        Current_Process[getPRID()] = NULL; // imposto il Current_process a NULL dato che il processo viene sospeso
        RELEASE_LOCK(&Global_Lock); 
        scheduler(); 

    }
}



/* 
    Verhogen(): la funzione ci permette di eseguire una operazione di .V() sul semaforo binario passato alla funzione attraverso il registro a0.
    Si controlla se sia possibile rimuovere il primo processo bloccato sul semaforo (ovviamente se il valore del semaforo è 0): se è possibile allora il processo viene sbloccato e inserito 
    nella coda dei processi "ready" (la ReadyQueue); se il valore del semaforo è 0 e sul semaforo non vi sono processi bloccati, si incrementa il valore di quest'ultimo. Altrimenti il processo 
    deve rimanere bloccato sul semaforo
*/
    
void Verhogen(state_t* syscallState, pcb_PTR corrente){

    memaddr* semaddr = (memaddr *)syscallState->reg_a1;
    pcb_PTR blockedProc = NULL;

    if (*semaddr == 0 && ((blockedProc = removeBlocked((int *)semaddr)) != NULL))
    { 
        syscallState->pc_epc += 4;  // salto di una word per evitare che la syscall vada in loop
        insertProcQ(&Ready_Queue, blockedProc); //inserisco il processo sbloccato nella ready queue
        RELEASE_LOCK(&Global_Lock); 
        LDST(syscallState); //ricarico lo stato precedente della cpu
    } else if (*semaddr == 0) // incremento il semaforo
    {
        *semaddr = 1;
        syscallState->pc_epc += 4;
        RELEASE_LOCK(&Global_Lock); 
        LDST(syscallState); //ricarico lo stato precedente della cpu
    } else { //il processo si blocca sul semaforo e viene chiamato lo scheduler
        syscallState->pc_epc += 4;
        corrente->p_s = *syscallState;
        corrente->p_semAdd = semaddr;
        int temp = insertBlocked((int*)semaddr, corrente);
        if (temp == 1) { //se non è possibile creare nuovi processi
            PANIC();
        }
        cpu_t now;
        STCK(now);
        Current_Process[getPRID()]->p_time += (now - Timestamp[getPRID()]); //incremento cpu time del processo corrente
        Current_Process[getPRID()] = NULL; // imposto il Current_process a NULL dato che il processo viene sospeso
        RELEASE_LOCK(&Global_Lock); 
        scheduler();
    }
}


/*
    DoIO(): funzione che permette di realizzare operazioni di I/O sui vari dispositivi (disk, flash, print, network e terminali).
    Per far partire un'operazione di I/O è necessario assegnare il valore salvato nel registro a2 al campo "Command" del device in questione, il cui
    indirizzo è salvato nel registro a1.

    E' importante tenere conto che i Terminal Device sono composti da due sotto-device, uno di trasmissione e uno di ricezione:
    per poter assegnare il valore del registro a2 al corretto campo "Command" bisogna riconoscere se il dispositivo terminale trasmette o riceve
    (e ciò viene fatto attraverso l'operazione "(commandAddr - TERM0ADDR)%0x10": siccome TERM0ADDR contiene l'indirizzo del primo dispositivo terminale,
    in questo modo, attraverso l'operazione di modulo, è possibile controllare se il risultato coincide con il valore "0xC" ossia il campo TRANSM_COMMAND
    oppure con il valore "0x4" ossia il campo RECV_COMMAND).

    Infine si calcolano il valore della linea e il numero del device per poter scrivere nel campo del device corretto.
*/

void DoIo(state_t* syscallState, pcb_PTR corrente) {
    int commandAddr = syscallState->reg_a1;
    int commandValue = syscallState->reg_a2;
    unsigned int base = commandAddr;
    int* semaddr = NULL;
    unsigned int pos = (commandAddr - TERM0ADDR)%0x10; //ricavo l'indirizzo del device register

    //se l'indirizzo di memoria commandAddr e' maggiore del valore base del TERM0 allora sicuramente commandAddr
    //e' un puntatore al campo COMMAND di un terminale e quindi ricaviamo l'indirizzo base del terminale
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

    unsigned int diff = base - 0x10000054; //0x10000054 è l'inizio della linea 3
    int linea =  diff/0x80 + 3; //la divisione permette di trovare il numero di linea corretto, da aggiungere poi alla linea 3 (siccome dalla 3 in avanti vi
                                //sono i device): 0x80 è il valore 128 (4 campi * 8 device * 4 word) in esadecimale.
    int device = (diff%(0x80))/(0x10); //il modulo ci permette di trovare il numero del dispositivo, siccome 0x80 è la grandezza di una linea intera
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
        case 7: //IL_TERMINAL
            if(pos == 0xC){
                semaddr = &SemaphoreTerminalTransmitter[device];
            } else{
                semaddr = &SemaphoreTerminalReceiver[device];
            } 
            break;
        default: //per sicurezza, se mai qualcosa andasse storto
            PANIC();
    }

    syscallState->pc_epc += 4;
    corrente->p_s = *syscallState;
    corrente->p_semAdd = semaddr;

    int temp = insertBlocked(semaddr, corrente);
    if (temp == 1) //se non è possibile creare nuovi processi
        PANIC();

    if(linea == 7)
    {
        if(pos == 0xC)
        {
            cpu_t now;
            STCK(now);
            Current_Process[getPRID()]->p_time += (now - Timestamp[getPRID()]); //incremento cpu time del processo corrente
            devAddrBase->term.transm_command = commandValue; //scriviamo il comando nel campo corretto del device
            Current_Process[getPRID()] = NULL;
            RELEASE_LOCK(&Global_Lock);     
            scheduler();
        }
        else
        {
            cpu_t now;
            STCK(now);
            Current_Process[getPRID()]->p_time += (now - Timestamp[getPRID()]); //incremento cpu time del processo corrente
            devAddrBase->term.recv_command = commandValue; //scriviamo il comando nel campo corretto del device
            Current_Process[getPRID()] = NULL;  
            RELEASE_LOCK(&Global_Lock);     
            scheduler();
        }
    }
    else
    {
        cpu_t now;
        STCK(now);
        Current_Process[getPRID()]->p_time += (now - Timestamp[getPRID()]); //incremento cpu time del processo corrente
        devAddrBase->dtp.command = commandValue; //scriviamo il comando nel campo corretto del device
        Current_Process[getPRID()] = NULL;  
        RELEASE_LOCK(&Global_Lock);     
        scheduler();
    }
}

/* la funzione GetCPUTime() ci permette di restituire il tempo d'esecuzione del processo che l’ha chiamata*/
void GetCPUTime(state_t* syscallState, pcb_PTR corrente) {
    cpu_t now;
    STCK(now);
    corrente->p_time += now - Timestamp[getPRID()];
    syscallState->reg_a0 = corrente->p_time; //restituisco il tempo d'esecuzione nell'a0 del chiamante
    syscallState->pc_epc += 4;
    STCK(Timestamp[getPRID()]); //metto nella timestamp il nuovo valore di inizio
    RELEASE_LOCK(&Global_Lock); 
    LDST(syscallState);
}

/* la funzione WaitForClock() ci permette di bloccare il processo corrente nell'ASL.*/
void WaitForClock(state_t* syscallState, pcb_PTR corrente) {
    memaddr* semaddr = &SemaphorePseudo; 
    syscallState->pc_epc += 4; // salto di una word per evitare che la syscall vada in loop
    corrente->p_s = *syscallState;
    corrente->p_semAdd = semaddr;
    int temp = insertBlocked((int*)semaddr, corrente); //blocco il processo corrente sul semaforo dello Pseudo clock
    if (temp == 1) //se non è possibile creare nuovi processi
        PANIC();

    cpu_t now;
    STCK(now);
    Current_Process[getPRID()]->p_time += (now - Timestamp[getPRID()]); //aggiorno il valore di vita del processo
    
    Current_Process[getPRID()] = NULL;
    RELEASE_LOCK(&Global_Lock); 
    scheduler();
}

/* la funzione GetSupportData() ci permette di restituire un puntatore alla support struct del processo current.*/
void GetSupportData(state_t* syscallState, pcb_PTR corrente) {
    support_t* support = corrente->p_supportStruct;
    syscallState->reg_a0 = (memaddr)support; //se non esiste NULL, sennò puntatore valido
    syscallState->pc_epc += 4;
    RELEASE_LOCK(&Global_Lock); 
    LDST(syscallState);
}

/*la funzione GetProcessID ci permette di restituire l'identificatore del chiamante se il parent è 0, altrimenti restituisce l'id del genitore*/
void GetProcessID(state_t* syscallState, pcb_PTR corrente) {

    if (syscallState->reg_a1 != 0) {
        if (corrente->p_parent != NULL) { // Se il processo corrente ha un padre
            syscallState->reg_a0 = corrente->p_parent->p_pid; // Restituisco il pid del padre
        } else {
            syscallState->reg_a0 = 0; // Se non esiste un padre, restituisco 0
        }
    } else {
        syscallState->reg_a0 = corrente->p_pid;
    }
    
    syscallState->pc_epc += 4;
    RELEASE_LOCK(&Global_Lock); 
    LDST(syscallState);
}

/*
    TLBHandlerExc(): l'eccezione avviene quando µRISC-V fallisce in una traduzione da un indirizzo logico ad uno fisico.
    Se il p_supportStruct del processo corrente e' uguale a NULL, allora viene richiamata la funzione Terminator che
    terminerà i processi del tree corrente. Altrimenti avviene un Pass Up.
*/

void TLBHandlerExc(state_t* syscallState, unsigned int cpuid){
    ACQUIRE_LOCK(&Global_Lock);
    pcb_PTR corrente = Current_Process[cpuid];

    if (corrente->p_supportStruct == NULL) //termino tutti i processi correlati al processo corrente e se stesso
    {
        Terminator(corrente);
        RELEASE_LOCK(&Global_Lock);
        scheduler();

    }
    else //corrente->p_supportStruct != NULL, passed up
    {
        corrente->p_supportStruct->sup_exceptState[PGFAULTEXCEPT] = *syscallState;
        context_t temp = corrente->p_supportStruct->sup_exceptContext[PGFAULTEXCEPT];
        RELEASE_LOCK(&Global_Lock);
        LDCXT(temp.stackPtr, temp.status, temp.pc);
    }
}

/*
    TRAPHandler(): la funzione viene richiamata quando avviene una istruzione illegale oppure non definita.
    Se il p_supportStruct del processore corrente è uguale a NULL, allora viene richiamata la funzione
    terminator che terminerà tutti i sotto-processi del tree del processo corrente.
    Altrimenti avviene un Pass Up.
*/

void TRAPHandler(state_t* syscallState, unsigned int cpuid) {
    ACQUIRE_LOCK(&Global_Lock);
    pcb_PTR corrente = Current_Process[cpuid];

    if (corrente->p_supportStruct == NULL) //termino tutti i processi correlati al processo corrente e se stesso
    {
        klog_print("TRAPHandler: No support struct, PID: ");
        klog_print_dec(corrente->p_pid);
        klog_print(" terminating");
        Terminator(corrente);
        RELEASE_LOCK(&Global_Lock);
        scheduler();
    }
    else //passed up
    {
        klog_print("TRAPHandler: Doing Pass Up to GeneralExceptionHandler");
        corrente->p_supportStruct->sup_exceptState[GENERALEXCEPT] = *syscallState;
        context_t temp = corrente->p_supportStruct->sup_exceptContext[GENERALEXCEPT];
        RELEASE_LOCK(&Global_Lock);
        LDCXT(temp.stackPtr, temp.status, temp.pc);
    }

}

/* Funzione richiamata da initial.c. Ha il compito di instradare l'eccezione avvenuta alla syscall corretta.*/
void SYSCALLHandler(state_t* syscallState, unsigned int cpuid){
    ACQUIRE_LOCK(&Global_Lock);
    pcb_PTR corrente = Current_Process[cpuid];

    if((syscallState->status & MSTATUS_MPP_MASK) != MSTATUS_MPP_M) //solo in kernel mode (se in user mode -> program trap)
    {   
        klog_print("sono qui");
        RELEASE_LOCK(&Global_Lock);
        TRAPHandler(syscallState, cpuid);    
        return;
    }

    switch(syscallState->reg_a0)
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
        
    default: //Trap
        RELEASE_LOCK(&Global_Lock);
        TRAPHandler(syscallState, cpuid);
        break;
    }
}

void TLBrefillHandler() {
    ACQUIRE_LOCK(&Global_Lock);
    unsigned int cpuid = getPRID();
    state_t *StatoCPU = GET_EXCEPTION_STATE_PTR(cpuid);
    
    unsigned int p = ENTRYHI_GET_VPN(StatoCPU->entry_hi); //una pagetable è fatta di 31 entry: la VPN (Virtual Page Number) mi permette di trovare il numero di pagina, ma non 
                                                         //conosco l'indirizzo che poi dovrò utilizzare per la privatePgTbl.
    
    pcb_PTR current = Current_Process[cpuid];
    support_t *Supporto = current->p_supportStruct;
    pteEntry_t Entry = Supporto->sup_privatePgTbl[p];
    setENTRYHI(Entry.pte_entryHI);
    setENTRYLO(Entry.pte_entryLO);
    TLBWR();
    RELEASE_LOCK(&Global_Lock);
    LDST(StatoCPU);
    
}