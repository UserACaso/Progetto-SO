#include "./headers/vmSupport.h"
static volatile unsigned int FIFO = 0;

void Pager(){
    support_t *sPtr = SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    

    if((sPtr->sup_exceptState[PGFAULTEXCEPT].cause & CAUSE_EXCCODE_MASK) == 24)
        P3TRAPHandler(sPtr, getPRID());
    else
    {
        SYSCALL(PASSEREN, &SwapTableSemaphore, 0, 0);
        unsigned int VirtAddress = sPtr->sup_exceptState[PGFAULTEXCEPT].entry_hi >> VPNSHIFT;
        unsigned int p_asid_new = sPtr->sup_asid;
        for(int i = 0; i < POOLSIZE; i++) //caso 6
        {
            if(SwapTable[i].sw_pageNo == VirtAddress && SwapTable[i].sw_asid == p_asid_new) {
                
                unsigned int oldStatus = getSTATUS();
                unsigned int newStatus = oldStatus & ~MSTATUS_MIE_MASK;
                setSTATUS(newStatus);
                setENTRYHI(sPtr->sup_exceptState[PGFAULTEXCEPT].entry_hi);
                TLBP(); //serve per controllare se il TLB è outdated
                setENTRYHI(SwapTable[i].sw_pte->pte_entryHI);
                setENTRYLO(SwapTable[i].sw_pte->pte_entryLO);
                TLBWI();
                setSTATUS(oldStatus);
                SYSCALL(VERHOGEN, &SwapTableSemaphore, 0, 0);
                LDST(&sPtr->sup_exceptState[PGFAULTEXCEPT]);
            } 
        }

        if(SwapTable[FIFO = (FIFO+1)%POOLSIZE].sw_asid != -1 && SwapTable[FIFO].sw_pageNo != -1) { //caso 9
                
            unsigned int oldStatus = getSTATUS();
            unsigned int newStatus = oldStatus & ~MSTATUS_MIE_MASK; //disabilitazione interrupt
            setSTATUS(newStatus);

            SwapTable[FIFO].sw_pte->pte_entryLO &= ~VALIDON; //invalidazione Table
            setENTRYHI(sPtr->sup_exceptState[PGFAULTEXCEPT].entry_hi);
            TLBP();
            if(!(getINDEX() & PRESENTFLAG))
            {
                setENTRYLO(SwapTable[FIFO].sw_pte->pte_entryLO); //invalidazione TLB
                TLBWI();
            }
            setSTATUS(oldStatus);
            
            //nuovo flash device su cui bisogna scrivere
            devreg_t* flash_device = (devreg_t*)(0x10000054 + ((IL_FLASH - 3) * 0x80) + ((SwapTable[FIFO].sw_asid - 1) * 0x10)); //calcolo indirizzo flash device
            flash_device->dtp.data0 = SwapPool[FIFO];
            unsigned int command = (SwapTable[FIFO].sw_pageNo << 8) | FLASHWRITE;
            SYSCALL(DOIO, &flash_device->dtp.command, command, 0);
            //program trap
        }
        //flash device vecchio su cui bisogna scrivere
        devreg_t* flash_device = (devreg_t*)(0x10000054 + ((IL_FLASH - 3) * 0x80) + ((p_asid_new - 1) * 0x10));
        flash_device->dtp.data0 = SwapPool[FIFO];
        unsigned int command = (SwapTable[FIFO].sw_pageNo << 8) | FLASHREAD;
        SYSCALL(DOIO, &flash_device->dtp.command, command, 0);
        //program trap
        SwapTable[FIFO].sw_asid = p_asid_new;
        SwapTable[FIFO].sw_pageNo = (VirtAddress == 0xBFFFF)? 31: VirtAddress-0x80000;
        SwapTable[FIFO].sw_pte = &sPtr->sup_privatePgTbl[SwapTable[FIFO].sw_pageNo];
        SwapTable[FIFO].sw_pte->pte_entryLO = ENTRYLO_VALID | (FIFO << ENTRYLO_PFN_BIT) | ENTRYLO_DIRTY; // dove N serve per non cache (1 bipassa, 0 non bipassa)
        
        unsigned int oldStatus = getSTATUS();
        unsigned int newStatus = oldStatus & ~MSTATUS_MIE_MASK;
        setSTATUS(newStatus);

        setENTRYHI(SwapTable[FIFO].sw_pte->pte_entryHI);
        TLBP();
        if(!(getINDEX() & PRESENTFLAG))
        {
            setENTRYLO(SwapTable[FIFO].sw_pte->pte_entryLO);
            TLBWI();
        }
        setSTATUS(oldStatus);

        SYSCALL(VERHOGEN, &SwapTableSemaphore, 0, 0);
        LDST(&sPtr->sup_exceptState[PGFAULTEXCEPT]);
    }
}
/*
    punto 6.
        controllo dell'intera swap pool, indipendentemente dal bit di validità

    Altrimenti

    punto 9.
        Bit di validità V=0: in RAM manca la pagina del TLB, quindi bisogna 
        spostare il frame corretto in RAM (ed eventualmente modificare l'indirizzo
        logico[?] del TLB)

 

*/