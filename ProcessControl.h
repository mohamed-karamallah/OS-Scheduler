#ifndef PROCESS_CONTROL_H
#define PROCESS_CONTROL_H

#include <sys/types.h>
#include "Data Structures/Process.h"

// Definition of ProcessControl structure
struct ProcessControl {
    pid_t pid;              // Process ID of the child
    int shmid;              // Shared memory ID for this process
    struct Process* data;   // Pointer to shared memory data
    int is_running;         // Flag if process is currently running
};


#endif // PROCESS_CONTROL_H 