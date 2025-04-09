#include "./headers/exceptions.h"

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
                -se è possibile eseguirla (ovviamente quando il valore del semaforo è 1!), allora la si esegue e si sblocca il processo. 
                -se non è possibile eseguirla, controlliamo se il valore del semaforo è 1, ed eventualmente decrementiamo;
                    -altrimenti inseriamo il processo nella lista dei processi bloccati del semaforo. ATTENZIONE: se non c'è più posto nella lista,
                     bisogna segnalare all'utente che non è possibile bloccare il processo (PANIC()).
*/

void Passeren(state_t* syscallState, int cpuid, pcb_PTR corrente){
    cpu_t current_time_inzio, current_time_fine;
    STCK(current_time_inzio);
    memaddr* semaddr = (memaddr *)syscallState->reg_a1; //semaddr contiene l'indirizzo del semaforo   
    pcb_PTR blockedProc = NULL;
    if (*semaddr == 1 && ((blockedProc = removeBlocked((int *)semaddr)) != NULL)){ //controlla se nella coda del semaforo se esiste una V: nel caso la sblocca e si sblocca anche questo processo
        insertProcQ(&Ready_Queue, blockedProc); //inserisco il processo sbloccato nella ready queue
        syscallState->pc_epc += 4;
        STCK(current_time_fine);
        corrente->p_time = current_time_fine - current_time_inzio;
        LDST(syscallState);
        RELEASE_LOCK(&Global_Lock); //fatto prima di richiamare lo scheduler
    } else if (*semaddr == 1){ // control is returned to the Current Process of the current processor
        *semaddr = 0; 
        syscallState->pc_epc += 4; 
        STCK(current_time_fine);
        corrente->p_time = current_time_fine - current_time_inzio;
        LDST(syscallState);
        RELEASE_LOCK(&Global_Lock);
    } else { //process is blocked on the ASL and the Scheduler is called
        syscallState->pc_epc += 4;
        corrente->p_s = *syscallState;


        
        //incremento cpu time del processo corrente

        int temp = insertBlocked(semaddr, corrente);
        if (temp == 1) //se non è possibile creare nuovi processi
        { /* gestione errore del semaforo */
            PANIC();
        }
        
        STCK(current_time_fine);
        corrente->p_time = current_time_fine - current_time_inzio;
        RELEASE_LOCK(&Global_Lock); 
        scheduler();

    }
    
}

void Verhogen(state_t* syscallState, int cpuid, pcb_PTR corrente){
    memaddr* semaddr = (memaddr *)syscallState->reg_a1;
    pcb_PTR blockedProc = NULL;
    if (*semaddr == 0 && ((blockedProc = removeBlocked((int *)semaddr)) != NULL)){ //controlla se nella coda del semaforo se esiste una P: nel caso la blocca e si blocca anche questo processo
        insertProcQ(&Ready_Queue, blockedProc); //inserisco il processo sbloccato nella ready queue
        syscallState->pc_epc += 4;
        LDST(syscallState);
        RELEASE_LOCK(&Global_Lock); 
    } else if (*semaddr == 0){// control is returned to the Current Process of the current processor
        *semaddr = 1;
        syscallState->pc_epc += 4;
        LDST(syscallState);
        RELEASE_LOCK(&Global_Lock); 
    } else { //process is blocked on the ASL and the Scheduler is called
        int temp = insertBlocked(semaddr, corrente);

        if (temp == 1) //se non è possibile creare nuovi processi
        { /* gestione errore del semaforo */
            PANIC();
        }
        
    }
    
}

void SYSCALLHandler(state_t* syscallState, unsigned int cpuid){
    ACQUIRE_LOCK(&Global_Lock);
    pcb_PTR corrente = Current_Process[cpuid];
    
    switch(syscallState->reg_a0) //only in kernel mode (if in user mode -> program trap)
    {
    case -1: //Create Process (SYS1)
        
        break;

    case -2: //Terminate Process (SYS2)
        
        break;
    
    case -3: //Passeren (P) 
        Passeren(syscallState, cpuid, corrente);
        
        break;

    case -4: //Verhogen (V)
        Verhogen(syscallState, cpuid, corrente);
        
        break;

    case -5: //DoIO (NSYS5)
        //ritorna
        break;
    case -6: //GetCPUTime (NSYS6)
        //ritorna p_supportStruct
        break;
    
    case -7: //WaitForClock (NSYS7)

        break;

    case -8: //GetSupportData (NSYS8)
        //ritorna
        break;
    
    case -9: //GetProcessID (NSYS9)
        //ritorna
        break;
    
    default: // Program Trap exception
        
        break;
    }
}

