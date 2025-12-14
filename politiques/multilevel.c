#include "process.h"


static int current_idx = -1;    
static int rr_time = 0;         
static int last_idx = -1;       


void policy_reset() {
    current_idx = -1;
    rr_time = 0;
    last_idx = -1;
}


int policy_select(Process *proc, int n, int time, int quantum) {
    

    for (int i = 0; i < n; i++) {
        if (proc[i].state == NEW && proc[i].arrival <= time) {
            proc[i].state = READY;
        }
    }

    int best_prio = -1;
    
    for (int i = 0; i < n; i++) {

        if ((proc[i].state == READY || proc[i].state == RUNNING) && proc[i].remaining > 0) {
            

            if (best_prio == -1 || proc[i].priority > best_prio) {
                best_prio = proc[i].priority;
            }
        }
    }


    if (best_prio == -1) {
        current_idx = -1;
        return -1;
    }


    if (current_idx != -1) {

        if (proc[current_idx].remaining <= 0) {
            proc[current_idx].state = FINISHED;
            proc[current_idx].end_time = time;
            current_idx = -1;
            rr_time = 0;
        }
       else if (proc[current_idx].priority < best_prio) {
            proc[current_idx].state = READY;

            current_idx = -1; 
            rr_time = 0;
        }

        else if (proc[current_idx].priority == best_prio) {
            if (rr_time >= quantum) {

                proc[current_idx].state = READY;
                current_idx = -1;
                rr_time = 0;
            } else {

                rr_time++;
                return current_idx;
            }
        }
    }

    int start_search = (last_idx + 1) % n;
    int candidate = -1;

    for (int i = 0; i < n; i++) {
        int idx = (start_search + i) % n;
        
        if ((proc[idx].state == READY || proc[idx].state == RUNNING) &&
            proc[idx].remaining > 0 &&
            proc[idx].priority == best_prio) { 
            
            candidate = idx;
            break; 
        }
    }


    if (candidate != -1) {
        current_idx = candidate;
        last_idx = candidate; 
        proc[current_idx].state = RUNNING;
        

        if (proc[current_idx].burst == proc[current_idx].remaining) {
            proc[current_idx].start_time = time;
        }
        
        rr_time = 1; 
        return current_idx;
    }

    return -1;
}
