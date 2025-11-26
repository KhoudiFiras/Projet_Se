#include "process.h"

/* Round-Robin state */
static int rr_index = -1;       // index du processus courant dans la ready queue
static int rr_time = 0;         // temps écoulé pour le quantum
static int queue[MAX_TIME];     // indices des processus dans la file
static int q_start = 0;         // début de la file
static int q_end = 0;           // fin de la file

/* Ajouter un processus à la fin de la file */
static void enqueue(int idx) {
    queue[q_end] = idx;
    q_end = (q_end + 1) % MAX_TIME;
}

/* Retirer le premier processus de la file */
static int dequeue() {
    if (q_start == q_end) return -1; // file vide
    int idx = queue[q_start];
    q_start = (q_start + 1) % MAX_TIME;
    return idx;
}

/* Vérifier si la file est vide */
static int queue_empty() {
    return q_start == q_end;
}

/* Round-Robin policy pour GTK */
int policy_select(Process *proc, int n, int time, int quantum) {
    // 1. Ajouter les nouveaux arrivants à la file
    for (int i = 0; i < n; i++) {
        if (proc[i].state == NEW && proc[i].arrival <= time) {
            proc[i].state = READY;
            enqueue(i);
        }
    }

    // 2. Si pas de processus en cours ou quantum écoulé, passer au suivant
    if (rr_index == -1 || rr_time >= quantum || proc[rr_index].state != RUNNING) {
        if (rr_index != -1 && proc[rr_index].state == RUNNING) {
            // remettre le processus courant à la fin s'il n'est pas fini
            if (proc[rr_index].remaining > 0) {
                proc[rr_index].state = READY;
                enqueue(rr_index);
            } else {
                proc[rr_index].state = FINISHED;
                proc[rr_index].end_time = time;
            }
        }
        rr_index = dequeue(); // prendre le suivant
        rr_time = 0;
    }

    // 3. Aucun processus prêt
    if (rr_index == -1) return -1;

    // 4. Le processus courant va s'exécuter
    proc[rr_index].state = RUNNING;
    rr_time++; // incrémenter le temps de quantum utilisé

    return rr_index;
}

/* Reset internal state */
void policy_reset() {
    rr_index = -1;
    rr_time = 0;
    q_start = 0;
    q_end = 0;
    // queue content doesn't strictly need clearing if indices are reset
}



