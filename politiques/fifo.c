#include "process.h"
/* FIFO: first arrived, first served (non preemptive simulated as choose ready first) */
int policy_select(Process *proc, int n, int time, int quantum) {
    for (int i=0;i<n;i++) {
        if (proc[i].state != FINISHED && proc[i].arrival <= time && proc[i].remaining > 0)
            return i;
    }
    return -1;
}

