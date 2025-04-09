#include "./headers/pcb.h"

static struct list_head pcbFree_h;
static pcb_t pcbFree_table[MAXPROC];
static int next_pid = 1;

void initPcbs() {
    //inizializziamo la lista dei pcb disponibili per l'allocazione
    INIT_LIST_HEAD(&pcbFree_h);
    for (int i = 0; i < MAXPROC; i++) {
        list_add(&pcbFree_table[i].p_list, &pcbFree_h);
    }
}

void freePcb(pcb_t* p) {
    //deallocazione pcb attualmente allocato
    list_add(&p->p_list, &pcbFree_h);
}


pcb_t* allocPcb() {
    //controllo se i pcb di una lista possono essere allocati o meno
    if (list_empty(&pcbFree_h)) {
        return NULL;
    }
    //inizializzazione parametri default
    pcb_t *pcb = container_of(pcbFree_h.prev, pcb_t, p_list);
    list_del(&pcb->p_list);
    INIT_LIST_HEAD(&pcb->p_child);
    INIT_LIST_HEAD(&pcb->p_sib);
    pcb->p_semAdd = NULL;
    pcb->p_parent = NULL;
    pcb->p_pid = next_pid++;
    pcb->p_time = 0;
    pcb->p_s.cause = 0;
    pcb->p_s.entry_hi= 0;
    pcb->p_s.mie = 0;
    pcb->p_s.pc_epc = 0;
    pcb->p_s.status = 0;
    pcb->p_supportStruct = NULL;
    pcb->p_time = 0;
    for(int i = 0; i < STATE_GPR_LEN; i++){
        pcb->p_s.gpr[i] = 0;
    }
    return pcb;
}

//inizializzazione dell'elemento sentinella di una coda di processi
void mkEmptyProcQ(struct list_head* head) {
    INIT_LIST_HEAD(head);
}

//controlla che la lista dei processi attivi sia vuota, ossia controlla che non vi siano processi.
int emptyProcQ(struct list_head* head) {
    return list_empty(head);
}

//inserisce un processo all'interno della lista dei processi attivi.
void insertProcQ(struct list_head* head, pcb_t* p) {
    list_add_tail(&p->p_list, head);
}

//ritorna la testa dei processi se la lista non e' vuota, altrimenti NULL
pcb_t* headProcQ(struct list_head* head) {
    if(emptyProcQ(head))
        return NULL;
    return container_of(head->next, pcb_t, p_list);
}

//rimozione di un processo dalla lista dei processi attivi.
pcb_t* removeProcQ(struct list_head* head) {
    if (emptyProcQ(head))
        return NULL;
    pcb_t *pcb = container_of(head->next, pcb_t, p_list);
    list_del(head->next);
    return pcb;
}
//rimozione del pcb puntato da p all'interno di una coda dei processi
//viene ritornato NULL se il processo non appartiene alla coda oppure se la coda e' vuota
pcb_t* outProcQ(struct list_head* head, pcb_t* p) {
    if (emptyProcQ(head))
        return NULL;
    struct list_head* iter;
    list_for_each(iter, head) {
        if(container_of(iter, pcb_t, p_list) == p) {
            list_del(iter);
            return p;
        }
    }
    return NULL;
}

//controlla se la lista figlio e' vuota
int emptyChild(pcb_t* p) {
    return list_empty(&p->p_child);
}

//inserisce un processo come figlio di un altro processo.
void insertChild(pcb_t* prnt, pcb_t* p)
{
    list_add_tail(&p->p_sib, &prnt->p_child);
    p->p_parent = prnt;
}

//rimozione dei processi figli di un processo padre.
//la funzione ritorna NULL se la lista dei figli e' vuota
pcb_t* removeChild(pcb_t* p) {
    if (emptyChild(p)) {
        return NULL;
    }

    pcb_t *figlio = container_of(p->p_child.next, pcb_t, p_sib);

    list_del(p->p_child.next);

    figlio->p_parent = NULL;
    return figlio;
}

//se il figlio ha un padre, il figlio non punta piu al padre e viceversa
pcb_t* outChild(pcb_t* p) {
    if(p->p_parent == NULL)
        return NULL;

    list_del(&p->p_sib);
    p->p_parent = NULL;
    return p;
}
