#include "./headers/scheduler.h"


void StopInterrupt()
{
    unsigned int *irt_entry = (unsigned int*) IRT_START;
    for (int i = 0; i < IRT_NUM_ENTRY; i++) {
    *irt_entry = getPRID(); 
    irt_entry++;
}
}


void scheduler(){
    //while (1){    
        ACQUIRE_LOCK(&Global_Lock);
        if(emptyProcQ(&Ready_Queue)){ //se ready queue è vuota
            if(Process_Count == 0){ //se non ci sono processi
                RELEASE_LOCK(&Global_Lock);
                StopInterrupt();
                HALT(); //invoco halt
            } else {
                RELEASE_LOCK(&Global_Lock);
                setMIE(MIE_ALL & ~MIE_MTIE_MASK); //disattivo plt interrupt
                unsigned int status = getSTATUS();
                status |= MSTATUS_MIE_MASK; //abilito gli interrupt
                setSTATUS(status);
                *((memaddr *) TPR) = 1; //metto cpu priorità bassa
                WAIT(); //metto in attesa
            }
        } else { //almeno un processo è pronto
            pcb_t *nextP = removeProcQ(&Ready_Queue); //rimuovo il primo da ready queue
            Current_Process[getPRID()] = nextP ; //getPRID mi dà id del processore. metto, quindi, il pointer alla pcb nel current process della cpu attuale
            STCK(Timestamp[getPRID()]);
            RELEASE_LOCK(&Global_Lock);
            setTIMER(TIMESLICE); //load 5ms nella plt  
            *((memaddr *) TPR) = 0; //metto cpu priorità alta
            LDST(&(nextP->p_s)); //faccio load processor state sul processor state della pcb in Current Process (p_s) della cpu attuale
        }
   // }
}
