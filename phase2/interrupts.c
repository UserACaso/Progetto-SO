#include "./headers/interrupts.h"

void InterruptHandler(unsigned int excode){
    ACQUIRE_LOCK(&Global_Lock);
    
   
    getMIP(); //machine interrupt pending
    getMIE(); //machine interrupt enabled
    // °°\_(* °)_/** MIP, MIE  32 bit

    //Non-Timer Interrupt:

    
    //Processor Local Timer (PLT) Interrupt


    //Interval Timer and the Pseudo-clock
    
    //Controllare come fare la bitmap per i vari device, 
    //gestione interrupt se tutti i processi sono gia occupati quindi con tLB messa a 0 
    //check di quale processore e' settato con la tbl a 1 
    //e far gestire a lui (confronto con 0...0 e tbl)
    
}