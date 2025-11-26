#include "process.h"

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
    queue[q_end] = idx;
    q_end = (q_end + 1) % MAX_TIME;
}


static int dequeue()
{
    if (q_start == q_end)
        return -1;   // queue empty

    int idx = queue[q_start];
    q_start = (q_start + 1) % MAX_TIME;
    return idx;
}


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
    int count = 0;
    for (int i = q_start; i != q_end; i = (i + 1) % MAX_TIME)
        count++;

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
            }
        }
    }

    // reset queue
    q_start = 0;
    q_end = 0;

    // push sorted items back
    for (int i = 0; i < count; i++)
        enqueue(temp[i]);
}


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
            proc[i].state = READY;
            enqueue(i);
        }
    }

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

            aging_index = dequeue();
        }
    }

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
    aging_index = -1;
    q_start = 0;
    q_end = 0;
}

