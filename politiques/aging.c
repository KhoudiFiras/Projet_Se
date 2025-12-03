#include "process.h"

/* Aging state */
static int aging_index = -1;    
static int queue[MAX_TIME];     
static int q_start = 0;         
static int q_end = 0;           
static int MAXP = 5;            
static int MINP = 0;            

/* Ajouter un processus à la fin de la file */
static void enqueue(int idx) {
    queue[q_end] = idx;
    q_end = (q_end + 1) % MAX_TIME;
}

/* Retirer le premier processus de la file */
static int dequeue() {
    if (q_start == q_end) return -1; 
    int idx = queue[q_start];
    q_start = (q_start + 1) % MAX_TIME;
    return idx;
}

/* Vérifier si la file est vide */
static int queue_empty() {
    return q_start == q_end;
}

/* Appliquer aging seulement au processus qui quitte le CPU */
static void apply_aging(Process *proc, int idx) {
    if (idx < 0) return;

    int p = proc[idx].priority;

    if (p % 2 == 0) { 
        p -= 1;
    } else {
        p -= 2; 
    }

    if (p <= 0) p = 1;

    proc[idx].priority = p;
}

/* Réorganiser la file par ordre de priorité + SRT */
static void reorder_queue(Process *proc) {
    if (queue_empty()) return;

    int count = 0;
    for (int i = q_start; i != q_end; i = (i + 1) % MAX_TIME)
        count++;

    int temp[count];
    int k = 0;

    for (int i = q_start; i != q_end; i = (i + 1) % MAX_TIME)
        temp[k++] = queue[i];

    /* Tri : 
       1) priorité décroissante
       2) remaining croissant (SRT)
    */
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            int a = temp[j];
            int b = temp[j + 1];

            if (proc[a].priority < proc[b].priority ||
               (proc[a].priority == proc[b].priority &&
                proc[a].remaining > proc[b].remaining)) {

                int sw = temp[j];
                temp[j] = temp[j + 1];
                temp[j + 1] = sw;
            }
        }
    }

    q_start = 0;
    q_end = 0;

    for (int i = 0; i < count; i++)
        enqueue(temp[i]);
}

/* Aging policy pour GTK */
int policy_select(Process *proc, int n, int time, int quantum) {

    /* 1. Ajouter les nouveaux arrivants */
    for (int i = 0; i < n; i++) {
        if (proc[i].state == NEW && proc[i].arrival <= time) {
            proc[i].state = READY;
            enqueue(i);
        }
    }

    /* 2. Réorganiser la file */
    reorder_queue(proc);

    /* 3. Si aucun processus ne tourne */
    if (aging_index == -1 || proc[aging_index].state != RUNNING) {
        aging_index = dequeue();
    }

    /* 4. Vérifier préemption */
    if (!queue_empty() && aging_index != -1) {
        int next = queue[q_start];

        if (proc[next].priority > proc[aging_index].priority ||
           (proc[next].priority == proc[aging_index].priority &&
            proc[next].remaining < proc[aging_index].remaining)) {

            /* appliquer aging sur celui qui quitte CPU */
            apply_aging(proc, aging_index);

            proc[aging_index].state = READY;
            enqueue(aging_index);

            aging_index = dequeue();
        }
    }

    /* 5. Si rien à exécuter */
    if (aging_index == -1) return -1;

    proc[aging_index].state = RUNNING;
    return aging_index;
}

/* Fonction d'initialisation */
void policy_init() {
    aging_index = -1;
    q_start = 0;
    q_end = 0;
}


