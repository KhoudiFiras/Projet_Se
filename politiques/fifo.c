#include "process.h"


int policy_select(Process *proc, int n, int time, int quantum) {
    int selected = -1;
    int min_arrival = 2147483647;

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

}

