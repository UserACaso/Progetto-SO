#include "./headers/interrupts.h"


/*
    Gestisce gli interrupt del Process Local Timer (PLT).
    Questa funzione viene chiamata quando il time-slice del processo corrente è scaduto: lo stato del processo che ha portato all'attivazione
    dell'interrupt viene impostato da "running" a "ready", e il TIMESLICE viene ricaricato.
    Dopodiché viene salvato lo stato del processo corrente, il quale viene rimesso nello Ready Queue.
 */

void PLTHandler(state_t* syscallState) {
    cpu_t now;
    ACQUIRE_LOCK(&Global_Lock);

    // Ricarica il timer per il prossimo time-slice
    setTIMER(TIMESLICE);

    int processore = getPRID();
    if (Current_Process[processore] != NULL)
    {
        STCK(now);
        pcb_PTR corrente = Current_Process[processore];

        // Salva lo stato del processo e aggiorna il tempo CPU utilizzato
        corrente->p_s = *syscallState;
        corrente->p_time += (now - Timestamp[processore]);

        // Transizione del processo da "running" a "ready"
        Current_Process[processore] = NULL;
        insertProcQ(&Ready_Queue, corrente);
    }

    STCK(Timestamp[getPRID()]);
    RELEASE_LOCK(&Global_Lock);
    scheduler();
}

/*
    Gestisce gli interrupt dello Pseudo-Clock (timer di sistema).
    Sblocca tutti i processi in attesa del tick dello pseudo-clock,
    inserendoli nella Ready Queue. Se non c'è un processo corrente
    altrimenti riprende l'esecuzione del processo corrente.
 */

void PseudoClockHandler(state_t* syscallState){
    ACQUIRE_LOCK(&Global_Lock);

    // Aggiorna il tempo CPU del processo corrente se presente
    if (Current_Process[getPRID()] != NULL)
    {
        cpu_t now;
        STCK(now);
        Current_Process[getPRID()]->p_time += (now - Timestamp[getPRID()]);
    }

    // Ricarica l'Interval Timer per il prossimo tick
    LDIT(PSECOND);

    int* indirizzo = &SemaphorePseudo;
    pcb_PTR blockedProc = NULL;
    while (*indirizzo == 0 && ((blockedProc = removeBlocked((int *)indirizzo)) != NULL)) // Sblocca tutti i processi in attesa dello pseudo-clock tick
    {
        insertProcQ(&Ready_Queue, blockedProc);
    }

    if (Current_Process[getPRID()] == NULL)
    {
        RELEASE_LOCK(&Global_Lock);
        scheduler(); // Richiama lo scheduler
    }else {
        STCK(Timestamp[getPRID()]);
        RELEASE_LOCK(&Global_Lock);
        LDST(syscallState);  // Riprende l'esecuzione del processo corrente
    }
}

/*
    Gestisce gli interrupt dei dispositivi esterni.
    Legge lo status del dispositivo che ha generato l'interrupt, fa l'ACK,
    e sblocca il processo che stava aspettando quel dispositivo.
    I terminal sono gestiti separatamente perché hanno due sub-dispositivi
    (trasmettitore e ricevitore).
 */

void DeviceHandler(int IntlineNo, int DevNo, state_t* syscallState){
    ACQUIRE_LOCK(&Global_Lock);

    // Aggiorna il tempo CPU del processo corrente se presente
    if (Current_Process[getPRID()] != NULL)
    {
        cpu_t now;
        STCK(now);
        Current_Process[getPRID()]->p_time += (now - Timestamp[getPRID()]);
    }

    // Calcola l'indirizzo base del device register
    devreg_t* devAddrBase = (devreg_t*)(0x10000054 + ((IntlineNo - 3) * 0x80) + (DevNo * 0x10));
    unsigned int status;
    int* indirizzo;

    if(IntlineNo != 7)  // Dispositivi non-terminal
    {
        status = devAddrBase->dtp.status;
        devAddrBase->dtp.command = ACK;

        // Determina quale semaforo usare in base alla linea del dispositivo
        switch (IntlineNo)
        {
            case 3:
                indirizzo = &SemaphoreDisk[DevNo];
                break;
            case 4:
                indirizzo = &SemaphoreFlash[DevNo];
                break;
            case 5:
                indirizzo = &SemaphoreNetwork[DevNo];
                break;
            case 6:
                indirizzo = &SemaphorePrinter[DevNo];
                break;
        }

        // Sblocca il processo in attesa su questo dispositivo
        pcb_PTR blockedProc = NULL;
        if (((blockedProc = removeBlocked((int *)indirizzo)) != NULL)){
            blockedProc->p_s.reg_a0 = status;  // Ritorna lo status nel registro a0
            blockedProc->p_semAdd = NULL;
            insertProcQ(&Ready_Queue, blockedProc);
        }
    }
    else  // Terminal devices
    {
        // Gestione interrupt trasmettitore
        if(devAddrBase->term.transm_status != 0 && devAddrBase->term.transm_status != 1 && devAddrBase->term.transm_status != 3)
        {
            indirizzo = &SemaphoreTerminalTransmitter[DevNo];
            status  = devAddrBase->term.transm_status;
            pcb_PTR blockedProc = NULL;
            if (((blockedProc = removeBlocked(indirizzo)) != NULL)){
                blockedProc->p_s.reg_a0 = status;
                blockedProc->p_semAdd = NULL;
                insertProcQ(&Ready_Queue, blockedProc);
            }
            devAddrBase->term.transm_command = ACK;
        }

        // Gestione interrupt ricevitore
        if(devAddrBase->term.recv_status != 0 && devAddrBase->term.recv_status != 1 && devAddrBase->term.recv_status != 3) {
            indirizzo = &SemaphoreTerminalReceiver[DevNo];
            status = devAddrBase->term.recv_status;
            pcb_PTR blockedProc = NULL;
            if (((blockedProc = removeBlocked(indirizzo)) != NULL)){
                blockedProc->p_s.reg_a0 = status;
                blockedProc->p_semAdd = NULL;
                insertProcQ(&Ready_Queue, blockedProc);
            }
            devAddrBase->term.recv_command = ACK;
        }
    }

    if (Current_Process[getPRID()] == NULL)
    {
        RELEASE_LOCK(&Global_Lock);
        scheduler(); // Richiama lo scheduler
    }else {
        STCK(Timestamp[getPRID()]);
        RELEASE_LOCK(&Global_Lock);
        LDST(syscallState); // Riprende l'esecuzione del processo corrente
    }
}

/*
    Determina il tipo di interrupt in base all'exception code e chiama
    l'handler appropriato. Per i dispositivi (linee 3-7), scansiona
    il bitmap degli interrupt pendenti per identificare quale dispositivo
    ha generato l'interrupt.
 */

void InterruptHandler(state_t* syscallState, unsigned int excode){
    unsigned int cause = getCAUSE();

    switch (excode)
    {
        case IL_CPUTIMER:
            PLTHandler(syscallState);
            break;

        case IL_TIMER:
            PseudoClockHandler(syscallState);
            break;

        default:
            // Scansione del bitmap degli interrupt pendenti per i dispositivi
            for (int i = 0; i < 5; i++) {
                memaddr* temp = 0x10000040 + (i*0x4);  // Indirizzo del bitmap per la linea i+3
                for(int dev = 0; dev < 8; dev++)
                {
                    if((*temp) & (1<<dev))  // Controlla se il bit del dispositivo è settato
                    {
                        DeviceHandler(i+3, dev, syscallState);
                    }
                }
            }
            break;
    }
}