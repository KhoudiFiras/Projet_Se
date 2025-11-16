#include "process.h"
static int rr_last = -1;
int policy_select(Process *proc, int n, int time, int quantum) {
    int start = (rr_last + 1) % n;
    for (int pass=0; pass<n; pass++) {
        int i = (start + pass) % n;
        if (proc[i].state != FINISHED && proc[i].arrival <= time && proc[i].remaining > 0) {
            rr_last = i;
            return i;
        }
    }
    return -1;
}

