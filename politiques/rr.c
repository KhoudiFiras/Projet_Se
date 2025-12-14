#include "process.h"


static int rr_index = -1;       
static int rr_time = 0;        
static int queue[MAX_TIME];    
static int q_start = 0;         
static int q_end = 0;          


static void enqueue(int idx) {
    queue[q_end] = idx;
    q_end = (q_end + 1) % MAX_TIME;
}


static int dequeue() {
    if (q_start == q_end) return -1; 
    int idx = queue[q_start];
    q_start = (q_start + 1) % MAX_TIME;
    return idx;
}


static int queue_empty() {
    return q_start == q_end;
}


int policy_select(Process *proc, int n, int time, int quantum) {

    for (int i = 0; i < n; i++) {
        if (proc[i].state == NEW && proc[i].arrival <= time) {
            proc[i].state = READY;
            enqueue(i);
        }
    }


    if (rr_index == -1 || rr_time >= quantum || proc[rr_index].state != RUNNING) {
        if (rr_index != -1 && proc[rr_index].state == RUNNING) {

            if (proc[rr_index].remaining > 0) {
                proc[rr_index].state = READY;
                enqueue(rr_index);
            } else {
                proc[rr_index].state = FINISHED;
                proc[rr_index].end_time = time;
            }
        }
        rr_index = dequeue(); 
        rr_time = 0;
    }


    if (rr_index == -1) return -1;


    proc[rr_index].state = RUNNING;
    rr_time++; 

    return rr_index;
}


void policy_reset() {
    rr_index = -1;
    rr_time = 0;
    q_start = 0;
    q_end = 0;

}



