#include "./headers/asl.h"


static semd_t semd_table[MAXPROC];
static struct list_head semdFree_h;
static struct list_head semd_h;

//inizializzazione e inserimento dei semafori all'interno della lista dei semafori disponibili
void initASL() {
    INIT_LIST_HEAD(&semdFree_h);
    INIT_LIST_HEAD(&semd_h);
    for (int i = 0; i < MAXPROC; i++) {
        list_add(&semd_table[i].s_link, &semdFree_h);
    }
}

//inserisce l'elemento p nella lista dei processi dei semafori con il giusto semAdd
//se il semaforo con il semAdd giusto non viene trovato, ne viene preso uno da semdFree_h
int insertBlocked(int* semAdd, pcb_t* p) {

    struct list_head *iter;
    semd_t *temp;
    list_for_each(iter, &semd_h) {
        temp = container_of(iter, semd_t, s_link);
        if (temp->s_key == semAdd) {
            list_add_tail(&p->p_list, &temp->s_procq);
            p->p_semAdd = semAdd;
            return 0;
        }
    }
    if(list_empty(&semdFree_h)) //se non è possibile creare nuovi processi occorre fermarsi.
        return 1;
    struct list_head *itr;
    semd_t *semaforo_free = container_of(semdFree_h.next, semd_t, s_link);
    list_del(semdFree_h.next);
    list_for_each(itr, &semd_h) { //si cerca la posizione in cui inserire il nuovo semaforo.
        semd_t *tmp = container_of(itr, semd_t, s_link);
        if (*tmp->s_key > *semAdd) {
            break;
        }
    }
    list_add_tail(&semaforo_free->s_link, itr);
    semaforo_free->s_key = semAdd;
    INIT_LIST_HEAD(&semaforo_free->s_procq);
    list_add_tail(&p->p_list, &semaforo_free->s_procq);
    p->p_semAdd = semAdd;

    return 0;
}

//dato un puntatore a un intero che identifica un semaforo la funzione rimuove il primo processo bloccato sul semaforo
pcb_t* removeBlocked(int* semAdd) {
    struct list_head *iter;
    list_for_each(iter, &semd_h) {
        semd_t *temp = container_of(iter, semd_t, s_link);

        //chiave trovata
        if (temp->s_key == semAdd) {
            //per il return del pcb_t
            pcb_t *ret = container_of(temp->s_procq.next, pcb_t, p_list);
            list_del(temp->s_procq.next);
            //reset numero semaforo
            ret->p_semAdd = NULL;
            //nel caso la coda del semaforo sia vuota il semaforo viene deallocato e rimesso nella lista dei semafori diponibili
            if (list_empty(&temp->s_procq)) {
                list_del(&temp->s_link);
                list_add(&temp->s_link,&semdFree_h);
            }
            return ret;
        }
    }
    return NULL;
}

//deallocazione di un processo identificato dal pid da un semaforo
//la funzione ritorna NULL se in nessun semaforo e presente quel processo
pcb_t* outBlockedPid(int pid) {
    struct list_head *iter;
    list_for_each(iter, &semd_h) { //doppio ciclo annidato per scorrere tutti i processi associati a ciascuno dei semafori.
        semd_t *temp = container_of(iter, semd_t, s_link);
        struct list_head *iter2;
        list_for_each(iter2, &temp->s_procq) {
            pcb_t *temp2 = container_of(iter2, pcb_t, p_list);
            if (temp2->p_pid == pid) {
                //se il processo e' l'ultimo all'interno della coda su questo semaforo viene invocata la funzione removeBlocked in modo da deallocare il semaforo e rimuovere il processo
                //se il processo invece non e' l'ultimo, viene rimosso solo il processo
                if (temp2->p_list.next == &temp->s_procq && temp2->p_list.prev == &temp->s_procq) {
                    removeBlocked(temp->s_key);
                } else {
                    list_del(&temp2->p_list);
                    temp2->p_semAdd = NULL;
                }
                return temp2;
            }
        }
    }
    return NULL;
}

//La outBlocked rimuove il processo p dalla asl su cui p è bloccato.
//prima individua il semaphore che gestisce il processo bloccato,
//poi il processo specifico nella coda associata a quel semaforo.
pcb_t* outBlocked(pcb_t* p) {
    struct list_head *iter;
    list_for_each(iter, &semd_h) { //itero per sem_h per cercare il semaforo associato a p
        semd_t *temp = container_of(iter, semd_t, s_link);
        if (temp->s_key==p->p_semAdd) {
            struct list_head *iter2; //itero nella sua coda dei processi del semaforo su cui p è bloccato
            list_for_each(iter2, &temp->s_procq){
                pcb_t *temp2=container_of(iter2,pcb_t,p_list);
                if (temp2==p){
                    list_del(iter2); //ho trovato il processo nella coda e lo rimuovo
                    temp2->p_semAdd = NULL;
                    if (list_empty(&temp->s_procq)) { //se la coda è vuota
                        list_del(&temp->s_link);
                        list_add(&temp->s_link, &semdFree_h); //lo metto nella lista semafori liberi
                    }
                    return temp2; //return del processo
                }
            }
        }
    }
    return NULL; //processo non trovato
}

// Restituisce un puntatore al PCB che è come testa alle process queue
pcb_t* headBlocked(int* semAdd) {
    struct list_head *iter;
    list_for_each(iter, &semd_h) { //itero nella semd_h
        semd_t *temp = container_of(iter, semd_t, s_link); //per confrontare il semaforo corrente con quello semAdd
        if (temp->s_key == semAdd) {
            if (list_empty(&temp->s_procq)) { //se la lista è vuota return NULL
                return NULL;
            }
            return container_of(temp->s_procq.next,pcb_t,p_list); //altrimenti restituisce il puntatore al primo PCB della coda
        }
    }
    return NULL; //non ha trovato semAdd
}

