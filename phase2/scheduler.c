#include "./headers/scheduler.h"
#include "klog.c"

void bp(){

}
void scheduler(){
    while (1)
    {
        ACQUIRE_LOCK(&Global_Lock);
        klog_print("bella ciao");
        klog_print_dec(getPRID());
        RELEASE_LOCK(&Global_Lock);
    }
}