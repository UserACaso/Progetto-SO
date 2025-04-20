#include "./headers/interrupts.h"

void PLTHandler() {

}

void PseudoClockHandler(){
    
}

void DeviceHandler(int IntlineNo, int DevNo){
    devreg_t* devAddrBase = 0x10000054 + ((IntlineNo - 3) * 0x80) + (DevNo * 0x10);
    unsigned int status;
    if(IntlineNo != 7)
    {
        status = devAddrBase->dtp.status;
        devAddrBase->dtp.command = ACK;
    }
    else
    {    
        status = devAddrBase->term.recv_status;
        devAddrBase->term.recv_command = ACK;
    }
    
    int indirizzo;
    switch (IntlineNo)
    {
        case 3: //IL_DISK 
            indirizzo = SemaphoreDisk[DevNo];
            break;
        case 4: //IL_FLASH
            indirizzo = SemaphoreFlash[DevNo];
            break;
        case 5: //IL_ETHERNET
            indirizzo = SemaphoreNetwork[DevNo];
            break;
        case 6: //IL_PRINTER
            indirizzo = SemaphorePrinter[DevNo];
            break;
        case 7: //IL_TERMINAL 
            indirizzo = SemaphoreTerminal[DevNo];
            break;
    }

    PassTest = 1;
    SYSCALL(VERHOGEN, &indirizzo, 0, 0);
    PassTest = 0; //si puo omettere se viene fatto gia dentro alla syscall
    pcb_t *proc = headBlocked(&indirizzo); 
    /*Qui abbiamo deciso di modificare la Veroghen e la Passeren, permettendo una sorta di "passaggio del testimone": se non usassimo
      questo espediente, con la chiamata di SYSCALL, avremmo un altro ACQUIRELOCK (cosa che vogliamo evitare).*/
      //NOTA COSA SUCEDE SE LA SYSCALL VEROGHEN SI BLOCCA SU UN SEMAFORO E VIENE CHIAMATO LO SCHEDULER (e' possibile?)
}

int DevNOGet(unsigned int Linea) {
    unsigned int temp = 0;
    for(int i = 0; i < 7; i++){
        if(*((memaddr *)(0x10000040) + (Linea-3)*0x4 ) & (DEV0ON << i))
        return i;
    }
}

void InterruptHandler(state_t* syscallState, unsigned int excode){
    ACQUIRE_LOCK(&Global_Lock);
    getMIP(); //machine interrupt pending
    //getMIE(); //machine interrupt enabled
    unsigned int cause = getCAUSE();
    for (int i = 0; i < 8; ++i) {
        unsigned int temp = 0;
        if ((temp = CAUSE_IP_GET(cause, i))) {
            switch (i) {
                case 1:
                    PLTHandler();
                    break;
                case 2:
                    PseudoClockHandler();
                    break;
                default:
                    DeviceHandler(i, DevNOGet(i));
                    break;
            }
            break;
        }
    }

    RELEASE_LOCK(&Global_Lock);

    //controllo che vi siano degli interrupt a priorità più alta (partendo dal basso)
    //se arriva un interrupt quando tutti i processori sono occupati, (e poi ne arriva un altra ancora)
    //Controllare come fare la bitmap per i vari device (per i device intlineno > 3, lo si fa)
    //gestione interrupt se tutti i processi sono gia occupati quindi con tLB messa a 0 
    //check di quale processore e' settato con la tbl a 1
    //e far gestire a lui (confronto con 0...0 e tbl)

}