#include "./headers/vmSupport.h"
static volatile unsigned int FIFO = 0;

void Pager(){
    support_t *sPtr = SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    

    if((sPtr->sup_exceptState[PGFAULTEXCEPT].cause & CAUSE_EXCCODE_MASK) == 24)
        P3TRAPHandler(sPtr, getPRID());
    else
    {
        SYSCALL(PASSEREN, &SwapTableSemaphore, 0, 0);
        unsigned int p = sPtr->sup_exceptState[PGFAULTEXCEPT].entry_hi >> VPNSHIFT;
        unsigned int p_asid = sPtr->sup_asid;
        for(int i = 0; i < POOLSIZE; i++)
        {
            if(SwapTable[i].sw_pageNo == p && SwapTable[i].sw_asid == p_asid) {
                setENTRYHI(sPtr->sup_exceptState[PGFAULTEXCEPT].entry_hi);
                TLBP(); //serve per controllare se il TLB è outdated
                setENTRYHI(SwapTable[i].sw_pte->pte_entryHI);
                setENTRYLO(SwapTable[i].sw_pte->pte_entryLO);
                TLBWI();
                SYSCALL(VERHOGEN, &SwapTableSemaphore, 0, 0);
                LDST(&sPtr->sup_exceptState[PGFAULTEXCEPT]);
            } 
            FIFO = (FIFO+1)POOLSIZE;
            if(SwapTable[FIFO].)
        }
        
    }


}
/*
    punto 6.
        controllo dell'intera swap pool, indipendenemente dal bit di validità

    Altrimenti

    punto 9.
        Bit di validità V=0: in RAM manca la pagina del TLB, quindi bisogna 
        spostare il frame corretto in RAM (ed eventualmente modificare l'indirizzo
        logico[?] del TLB)



*/