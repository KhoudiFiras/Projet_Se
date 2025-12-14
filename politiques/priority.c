#include "process.h"
int policy_select(Process *proc, int n, int time, int quantum) {
    int best = -1; int bestprio = 1<<30;
    for (int i=0;i<n;i++) {
        if (proc[i].state != FINISHED && proc[i].arrival <= time && proc[i].remaining > 0) {
            if (proc[i].priority < bestprio) { bestprio = proc[i].priority; best = i; }
        }
    }
    return best;
}

void policy_reset() {

}

