#ifndef PROCESS_H
#define PROCESS_H

#define MAX_NAME 32
#define MAX_TIME 2000   

typedef enum { NEW, READY, RUNNING, FINISHED } State;

typedef struct {
    char name[MAX_NAME];
    int arrival;
    int burst;
    int remaining;
    int priority;      
    int start_time;
    int end_time;
    int waited;
    State state;
} Process;

#endif 
