#include "./headers/scheduler.h"

/*
    Lo scheduler svolge la funzione essenziale di scheduling dei processi, ovvero si occupa di stabilire l'ordine
    di esecuzione dei processi. Decide quale processo va eseguito dalla CPU e per quanto tempo.
    Questo scheduler utilizza un Round-Robin preemptivo: ogni processo nella ready queue può usare la CPU per un intervallo di tempo fisso di 5ms
    (intervallo definito tramite il PLT).
    Se la ReadyQueue è vuota e non ci sono processi attivi, il sistema ha completato l'esecuzione di tutti i processi viene invocata HALT.
    Se ci sono processi bloccati e non c'è nessun processo pronto, la CPU viene messa in WAIT, finché non arriva un interrupt che sia diverso da quello del PLT (che viene disabilitato)
    Se almeno un processo è pronto, viene rimosso dalla ready queue, il suo pointer viene assegnato al current process della CPU attuale, quindi viene impostato il time slice col timer.
 */

void scheduler(){
    ACQUIRE_LOCK(&Global_Lock);
    if(emptyProcQ(&Ready_Queue)){ //se ready queue è vuota
        if(Process_Count == 0){ //se non ci sono processi attivi
            RELEASE_LOCK(&Global_Lock);
            HALT(); //invoco halt
        } else { //ci sono processi bloccati e nessuno pronto
            RELEASE_LOCK(&Global_Lock);
            setMIE(MIE_ALL & ~MIE_MTIE_MASK); //disattivo plt interrupt
            unsigned int status = getSTATUS();
            status |= MSTATUS_MIE_MASK; //abilito gli altri interrupt
            setSTATUS(status);
            *((memaddr *) TPR) = 1; //metto cpu priorità bassa
            WAIT(); //metto in attesa
        }
    } else { //almeno un processo è pronto
        pcb_t *nextP = removeProcQ(&Ready_Queue); //rimuovo il primo da ready queue
        Current_Process[getPRID()] = nextP ; //getPRID mi dà id del processore. Metto, quindi, il pointer alla pcb nel current process della cpu attuale
        STCK(Timestamp[getPRID()]);
        RELEASE_LOCK(&Global_Lock);
        setTIMER(TIMESLICE); //load 5ms nella plt  
        *((memaddr *) TPR) = 0; //metto cpu priorità alta
        LDST(&(nextP->p_s)); //faccio load processor state sul processor state della pcb in Current Process (p_s) della cpu attuale
    }
}
