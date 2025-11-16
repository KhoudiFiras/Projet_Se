#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "process.h"

/* Parse config file: format "NAME ARRIVAL BURST PRIORITY" lines.
   Ignore lines starting with '#' and blank lines. Returns count or -1 on error.
*/
int parse_config(const char *path, Process *arr, int maxp) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[256];
    int count = 0;
    while (fgets(line, sizeof(line), f)) {
        char *s = line;
        while (*s == ' ' || *s == '\t') s++;
        if (*s == '#' || *s == '\n' || *s == '\0') continue;
        char name[64];
        int arrival, burst, prio;
        if (sscanf(line, "%63s %d %d %d", name, &arrival, &burst, &prio) == 4) {
            if (count >= maxp) break;
            strncpy(arr[count].name, name, MAX_NAME-1);
            arr[count].name[MAX_NAME-1] = '\0';
            arr[count].arrival = arrival;
            arr[count].burst = burst;
            arr[count].remaining = burst;
            arr[count].priority = prio;
            arr[count].start_time = -1;
            arr[count].end_time = -1;
            arr[count].waited = 0;
            arr[count].state = NEW;
            count++;
        }
    }
    fclose(f);
    return count;
}

