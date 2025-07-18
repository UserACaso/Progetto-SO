#include "./headers/vmSupport.h"

static volatile unsigned int FIFO = 0;

void Pager(){
    support_t *sPtr = SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    
    // Punto 2-3: Determinare la causa dell'eccezione TLB
    if((sPtr->sup_exceptState[PGFAULTEXCEPT].cause & CAUSE_EXCCODE_MASK) == EXC_MOD) {
        // TLB-Modification exception - trattare come program trap
        P3TRAPHandler(sPtr);
    }
    else {
        // Punto 4: Acquisire mutua esclusione sulla Swap Pool table
        SYSCALL(PASSEREN, &SwapTableSemaphore, 0, 0);
        
        // Punto 5: Determinare il numero della pagina mancante
        unsigned int pageNo = ENTRYHI_GET_VPN(sPtr->sup_exceptState[PGFAULTEXCEPT].entry_hi);
        unsigned int p_asid = sPtr->sup_asid;
        
        // Punto 6: Controllare se la pagina è già caricata nella swap pool
        for(int i = 0; i < POOLSIZE; i++) {
            if(SwapTable[i].sw_asid == p_asid && SwapTable[i].sw_pageNo == pageNo) {
                // La pagina è già nella swap pool, aggiornare il TLB se necessario
                unsigned int oldStatus = getSTATUS();
                unsigned int newStatus = oldStatus & ~MSTATUS_MIE_MASK;
                setSTATUS(newStatus);
                
                // Punto 6a: Controllare se l'entry è nel TLB
                setENTRYHI(sPtr->sup_exceptState[PGFAULTEXCEPT].entry_hi);
                TLBP();
                
                // Punto 6b: Se l'entry è nel TLB (P=0), aggiornala
                if(!(getINDEX() & PRESENTFLAG)) {
                    setENTRYHI(SwapTable[i].sw_pte->pte_entryHI);
                    setENTRYLO(SwapTable[i].sw_pte->pte_entryLO);
                    TLBWI();
                }
                
                setSTATUS(oldStatus);
                
                // Punto 6c: Rilasciare mutua esclusione e ritornare
                SYSCALL(VERHOGEN, &SwapTableSemaphore, 0, 0);
                LDST(&sPtr->sup_exceptState[PGFAULTEXCEPT]);
            }
        }
        
        // Punto 7: Scegliere un frame dalla Swap Pool
        unsigned int frameToUse = FIFO;
        FIFO = (FIFO + 1) % POOLSIZE;
        
        // Punto 8-9: Determinare se il frame è occupato
        if(SwapTable[frameToUse].sw_asid != -1) {
            // Il frame è occupato, deve essere liberato
            unsigned int oldStatus = getSTATUS();
            unsigned int newStatus = oldStatus & ~MSTATUS_MIE_MASK;
            setSTATUS(newStatus);
            
            // Punto 9a: Aggiornare la Page Table del processo che occupa il frame
            SwapTable[frameToUse].sw_pte->pte_entryLO &= ~VALIDON;
            
            // Punto 9b: Aggiornare il TLB se necessario
            setENTRYHI(SwapTable[frameToUse].sw_pte->pte_entryHI);
            TLBP();
            if(!(getINDEX() & PRESENTFLAG)) {
                setENTRYLO(SwapTable[frameToUse].sw_pte->pte_entryLO);
                TLBWI();
            }
            
            setSTATUS(oldStatus);
            
            // Punto 9c: Aggiornare il backing store (scrivere su flash)
            int deviceNo = SwapTable[frameToUse].sw_asid - 1;  // ASID 1-8 → Device 0-7
            devreg_t* flash_device = DEV_REG_ADDR(IL_FLASH, deviceNo);
            flash_device->dtp.data0 = SwapPool[frameToUse];
            unsigned int command = (SwapTable[frameToUse].sw_pageNo << 8) | FLASHWRITE;
            int status = SYSCALL(DOIO, &flash_device->dtp.command, command, 0);
            
            // Gestire errori I/O come program trap
            if(status != READY) {
                SYSCALL(VERHOGEN, &SwapTableSemaphore, 0, 0);
                P3TRAPHandler(sPtr);
            }
        }
        
        // Punto 10: Leggere dal backing store del processo corrente
        int deviceNo = p_asid - 1;  // ASID 1-8 → Device 0-7
        devreg_t* flash_device = DEV_REG_ADDR(IL_FLASH, deviceNo);
        flash_device->dtp.data0 = (memaddr)SwapPool[frameToUse];
        unsigned int command = (pageNo << 8) | FLASHREAD;
        int status = SYSCALL(DOIO, &flash_device->dtp.command, command, 0);
        
        // Gestire errori I/O come program trap
        if(status != READY) {
            SYSCALL(VERHOGEN, &SwapTableSemaphore, 0, 0);
            P3TRAPHandler(sPtr);
        }
        
        // Punto 11: Aggiornare la Swap Pool table
        SwapTable[frameToUse].sw_asid = p_asid;
        SwapTable[frameToUse].sw_pageNo = pageNo;
        SwapTable[frameToUse].sw_pte = &sPtr->sup_privatePgTbl[pageNo];
        
        // Punto 12: Aggiornare la Page Table entry
        SwapTable[frameToUse].sw_pte->pte_entryLO = VALIDON | DIRTYON | (frameToUse << ENTRYLO_PFN_BIT);
        
        // Punto 13: Aggiornare il TLB
        unsigned int oldStatus = getSTATUS();
        unsigned int newStatus = oldStatus & ~MSTATUS_MIE_MASK;
        setSTATUS(newStatus);
        
        setENTRYHI(SwapTable[frameToUse].sw_pte->pte_entryHI);
        TLBP(); // Cerca se l'entry è già nel TLB
        
        if(!(getINDEX() & PRESENTFLAG)) {
            // Entry trovata (P=0), aggiorna lo slot esistente
            setENTRYLO(SwapTable[frameToUse].sw_pte->pte_entryLO);
            TLBWI();
        } else {
            // Entry non trovata (P=1), scrivi in uno slot random
            setENTRYLO(SwapTable[frameToUse].sw_pte->pte_entryLO);
            TLBWR();
        }
        
        setSTATUS(oldStatus);
        
        // Punto 14: Rilasciare mutua esclusione
        SYSCALL(VERHOGEN, &SwapTableSemaphore, 0, 0);
        
        // Punto 15: Ritornare controllo al processo
        LDST(&sPtr->sup_exceptState[PGFAULTEXCEPT]);
    }
}