#include "process.h"

<<<<<<< HEAD
/* Aging scheduling state */
static int aging_index = -1;          // currently running process index
static int queue[MAX_TIME];           // ready queue (process indices)
static int q_start = 0;               // queue head
static int q_end = 0;                 // queue tail

static int MAXP = 5;                  // maximum priority
static int MINP = 0;                  // minimum priority


/* =============================
   Queue Management
   ============================= */

static void enqueue(int idx)
{
=======
/* Aging state */
static int aging_index = -1;    
static int queue[MAX_TIME];     
static int q_start = 0;         
static int q_end = 0;           
static int MAXP = 5;            
static int MINP = 0;            

/* Ajouter un processus à la fin de la file */
static void enqueue(int idx) {
>>>>>>> origin/main
    queue[q_end] = idx;
    q_end = (q_end + 1) % MAX_TIME;
}

<<<<<<< HEAD

static int dequeue()
{
    if (q_start == q_end)
        return -1;   // queue empty

=======
/* Retirer le premier processus de la file */
static int dequeue() {
    if (q_start == q_end) return -1; 
>>>>>>> origin/main
    int idx = queue[q_start];
    q_start = (q_start + 1) % MAX_TIME;
    return idx;
}

<<<<<<< HEAD

static int queue_empty()
{
    return (q_start == q_end);
}


/* =============================
   Aging Priority Update
   ============================= */

static void update_priorities(Process *proc, int n)
{
    for (int i = 0; i < n; i++)
    {
        if (proc[i].state == READY || proc[i].state == RUNNING)
        {
            // basic alternating priority decrease
            if (i % 2 == 0)
            {
                if (proc[i].priority > MINP)
                    proc[i].priority--;
            }
            else
            {
                if (proc[i].priority > MINP)
                    proc[i].priority = (proc[i].priority - 2 > MINP)
                        ? proc[i].priority - 2
                        : MINP;
            }

            // slowly increase if minimal
            if (proc[i].priority == MINP && proc[i].priority < MAXP)
                proc[i].priority++;

            // clamp
            if (proc[i].priority < MINP) proc[i].priority = MINP;
            if (proc[i].priority > MAXP) proc[i].priority = MAXP;
        }
    }
}


/* =============================
   Queue reorder by priority
   ============================= */

static void reorder_queue(Process *proc)
{
    if (queue_empty())
        return;

    // count items
=======
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

>>>>>>> origin/main
    int count = 0;
    for (int i = q_start; i != q_end; i = (i + 1) % MAX_TIME)
        count++;

<<<<<<< HEAD
    // extract
    int temp[count];
    int idx = 0;

    for (int i = q_start; i != q_end; i = (i + 1) % MAX_TIME)
        temp[idx++] = queue[i];

    // bubble sort by descending priority
    for (int i = 0; i < count - 1; i++)
    {
        for (int j = 0; j < count - i - 1; j++)
        {
            if (proc[temp[j]].priority < proc[temp[j + 1]].priority)
            {
                int swap = temp[j];
                temp[j] = temp[j + 1];
                temp[j + 1] = swap;
=======
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
>>>>>>> origin/main
            }
        }
    }

<<<<<<< HEAD
    // reset queue
    q_start = 0;
    q_end = 0;

    // push sorted items back
=======
    q_start = 0;
    q_end = 0;

>>>>>>> origin/main
    for (int i = 0; i < count; i++)
        enqueue(temp[i]);
}

<<<<<<< HEAD

/* =============================
   Policy Core Function
   ============================= */

int policy_select(Process *proc, int n, int time, int quantum)
{
    // 1) add new arrivals
    for (int i = 0; i < n; i++)
    {
        if (proc[i].state == NEW && proc[i].arrival <= time)
        {
=======
/* Aging policy pour GTK */
int policy_select(Process *proc, int n, int time, int quantum) {

    /* 1. Ajouter les nouveaux arrivants */
    for (int i = 0; i < n; i++) {
        if (proc[i].state == NEW && proc[i].arrival <= time) {
>>>>>>> origin/main
            proc[i].state = READY;
            enqueue(i);
        }
    }

<<<<<<< HEAD
    // 2) update priorities
    update_priorities(proc, n);

    // 3) reorder queue
    reorder_queue(proc);

    // 4) choose next if none running
    if (aging_index == -1 || proc[aging_index].state != RUNNING)
    {
        if (aging_index != -1 && proc[aging_index].state == RUNNING)
        {
            if (proc[aging_index].remaining > 0)
            {
                proc[aging_index].state = READY;
                enqueue(aging_index);
            }
            else
            {
                proc[aging_index].state = FINISHED;
                proc[aging_index].end_time = time;
            }
        }

        aging_index = dequeue();
    }

    // 5) preemption
    if (aging_index != -1 && !queue_empty())
    {
        int next_priority = proc[queue[q_start]].priority;

        if (next_priority > proc[aging_index].priority)
        {
            if (proc[aging_index].state == RUNNING)
            {
                proc[aging_index].state = READY;
                enqueue(aging_index);
            }
=======
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
>>>>>>> origin/main

            aging_index = dequeue();
        }
    }

<<<<<<< HEAD
    // 6) no ready process
    if (aging_index == -1)
        return -1;

    // 7) run process
    proc[aging_index].state = RUNNING;

    return aging_index;
}


/* =============================
   Reset state
   ============================= */

void policy_init()
{
=======
    /* 5. Si rien à exécuter */
    if (aging_index == -1) return -1;

    proc[aging_index].state = RUNNING;
    return aging_index;
}

/* Fonction d'initialisation */
void policy_init() {
>>>>>>> origin/main
    aging_index = -1;
    q_start = 0;
    q_end = 0;
}

<<<<<<< HEAD
=======

>>>>>>> origin/main
