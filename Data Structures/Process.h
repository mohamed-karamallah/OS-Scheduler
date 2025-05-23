//processh
#ifndef PROCESS_H
#define PROCESS_H

#define STATE_READY     "ready"
#define STATE_RUNNING   "running"
#define STATE_BLOCKED   "blocked"
#define STATE_RESUMED   "resumed" 
#define STATE_FINISHED  "finished"
#define STATE_STOPPED   "stopped"
#define STATE_ARRIVED   "arrived"
#define STATE_STARTED   "started"

struct Process {
    int id;
    int arrival_time;
    int run_time;
    int priority;
    int remaining_time;
    int waiting_time;
    int start_time;         
    int finish_time;        
    int last_stop_time; 
    int preemption_count; 

    char state[20];    
    // Memory management fields
    int memsize;      // Size of memory required by the process
    int mem_start;    // Start index in memory (set by buddy allocator)
    int mem_end;      // End index in memory (set by buddy allocator)
};

#endif