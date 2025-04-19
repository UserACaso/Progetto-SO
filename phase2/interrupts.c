#include "./headers/interrupts.h"

void PLTHandler() {

}

void PseudoClockHandler(){
    
}

void DeviceHandler(int IntlineNo){
}

int DevNOGet(unsigned int Devices) {
    //Stavo pensando a una cosa: se, per esempio, abbiamo un Interrupt nella Linea 3 dipositivo 0: quando controlliamo altrin interrupt
    //pendenti non possono essercene altri in quella stessa area di memoria (ossia Linea 3, dispositivo 0) finché quell'interrupt non viene risolta.
    //Credo eh.

    
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
                case IL_CPUTIMER:
                    PLTHandler();
                    break;
                case IL_TIMER:
                    PseudoClockHandler();
                    break;
                default:
                    //
                    DeviceHandler(i);
                    break;
            }
            break;
        }
    }
    // for (int i = 0; i < 7; i++)
    // {
    //     if (i == 0 && getTIMER() == 0 ){
    //         //gestione interrupt della linea 1: PLT (Processor Local Timer)
    //         PLTHandler();
    //         break;
    //     } else if (i == 1 && (*((cpu_t *)INTERVALTMR)) == 0) {
    //         //gestione interrupt della linea 1: Pseudo-Clock (riguarda tutte le CPU)
    //         PseudoClockHandler();
    //         break;
    //     } else if (i >=2 && (bitMask = *((memaddr *)(0x10000040 + (0x04) * (i-2)))) && bitMask != 0){ //indirizzo fisico della linea del device
    //         unsigned int temp = 0;
    //         for (int i = 0; i < 7; ++i) {
    //             if (i == 0)
    //             if ((temp = bitMask & DEV0ON << i)) //calcolare il device corrispondente magari utilizzare CAUSE_IP
    //         }
    //
    //         DeviceHandler(bitMask, i+1);
    //         break;
    //     }
    //     CAUSE_IP_GET(cause, 3); //controlla se vi sono degli interrupt pendenti della linea 3.
    // }

    RELEASE_LOCK(&Global_Lock);

    //controllo che vi siano degli interrupt a priorità più alta (partendo dal basso)
    //se arriva un interrupt quando tutti i processori sono occupati, (e poi ne arriva un altra ancora)
    //Controllare come fare la bitmap per i vari device (per i device intlineno > 3, lo si fa)
    //gestione interrupt se tutti i processi sono gia occupati quindi con tLB messa a 0 
    //check di quale processore e' settato con la tbl a 1
    //e far gestire a lui (confronto con 0...0 e tbl)

}