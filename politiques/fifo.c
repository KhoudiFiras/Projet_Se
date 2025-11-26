#include "process.h"

/* FIFO: first arrived, first served */
int policy_select(Process *proc, int n, int time, int quantum) {
    int selected = -1;
    int min_arrival = 2147483647;

    /* First, check if there is a process currently running (non-preemptive) */
    /* Actually, the simulation loop sets state to READY if we switch, so we can just look for the best candidate.
       But for strict FIFO, we should prioritize the one that arrived earliest. */

    for (int i=0; i<n; i++) {
        if (proc[i].state != FINISHED && proc[i].arrival <= time && proc[i].remaining > 0) {
            if (proc[i].arrival < min_arrival) {
                min_arrival = proc[i].arrival;
                selected = i;
            }
        }
    }
    return selected;
}

void policy_reset() {
    // No internal state to reset for FIFO
}

