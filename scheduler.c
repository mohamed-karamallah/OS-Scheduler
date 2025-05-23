//scheduler
#include "headers.h"
#include "Data Structures/PriQueue.h"
#include "Data Structures/Queue.h"
#include "memory.c"
#define LOG_FILE_NAME "scheduler.log"
#define PERF_FILE_NAME "scheduler.perf"
#include "Data Structures/CircularQ.h"
#include <stdlib.h>
struct Parameters {
    int completed_processes;
    int cpu_busy_time;
    double total_wta;
    double total_waiting;
    double total_wta_squared;
};
// Global variables
int clk;
int last_clk;
int msg_id;
int scheduler_pid;
struct PriQueue ready_queue;
FILE *log_file;
FILE *perf_file;
int total_processes = 0;
bool process_finished = true;

struct Process* rr_current_process = NULL;  
pid_t rr_current_pid = -1;                  
bool rr_needs_scheduling = true;            // flag to indicate if a new process needs scheduling

struct Parameters *pra;
struct Queue QueueSTRN;
struct QueueQ circQ;
int quantum;
struct Parameters* para;
struct Process* shared_process;
void process_finished_handler(int signum);
void try_allocate_waiting_processes(int current_time, int algorithm);
int shmid, shmid2;
struct Queue waiting_queue;

// Memory manager for buddy allocation
struct MemoryManager memory_manager;

// Waiting list for processes that couldn't be allocated memory
struct Queue waiting_mem_queue;

char* to_string(int value);

// to create a new process
pid_t create_process(struct Process* p) {
    // get shared memory segment for process 
    int shm_key2 = ftok("./tmp.txt", 'P');
    if (shm_key2 == -1) {
        perror("Error generating key for process shared memory");
        return -1;
    }
    
    shmid2 = shmget(shm_key2, sizeof(struct Process), IPC_CREAT | 0666);
    if (shmid2 == -1) {
        perror("Error creating shared memory for process");
        return -1;
    }
    
    shared_process = (struct Process*)shmat(shmid2, NULL, 0);
    if (shared_process == (void*)-1) {
        perror("Error attaching shared memory for process");
        return -1;
    }
    
    // copy process data to shared memory
    memcpy(shared_process, p, sizeof(struct Process));
    
   
    pid_t pid = fork();
   
    if (pid == -1) {
        perror("Error forking process");
        return -1;
    }
    
    if (pid == 0) {
        
        char scheduler_pid_str[16];
        sprintf(scheduler_pid_str, "%d", scheduler_pid);
        
        
        char quantum_str[16] = "-1";
        
        printf("Starting process with ID=%d, arrival=%d, runtime=%d, priority=%d\n", 
               p->id, p->arrival_time, p->run_time, p->priority);
        
        execl("./process.out", "process.out", scheduler_pid_str, quantum_str, NULL);
              
        
        perror("Error executing process");
        exit(EXIT_FAILURE);
    }
    
   
    return pid;
}

// to stop a running process
void stop_process(pid_t pid) {
    if (pid > 0) {
        if (kill(pid, SIGTSTP) == -1) {
            perror("Error stopping process");
        }
    }
}

// to resume a stopped process
void resume_process(pid_t pid) {
    if (pid > 0) {
        if (kill(pid, SIGCONT) == -1) {
            perror("Error resuming process");
        }
    }
}

// to terminate a process
void terminate_process(pid_t pid) {
    if (pid > 0) {
        if (kill(pid, SIGTERM) == -1) {
            perror("Error terminating process");
        }
    }
}

// signal handler for process completion
void process_finished_handler(int signum) {
    printf("Process completion signal received at time %d\n", clk);
    
    process_finished = true;
}

void logProcessState(int current_time, int pid, const char* action, struct Process* p) {
    printf("Logging: At time %d, process %d, %s\n", current_time, pid, action);
    
    static bool header_written = false;
    
    // Open log file if not already open
    if (!log_file) {
        log_file = fopen(LOG_FILE_NAME, "a");
        if (!log_file) {
            perror("Failed to open log file");
            return;
        }
        
        // Write header if file is empty
        if (!header_written) {
            fprintf(log_file, "#At time x process y state arr w total z remain y wait k\n\n");
            header_written = true;
        }
    }

    fprintf(log_file, "At time %d process %d %s arr %d total %d remain %d wait %d",
            current_time, pid, action, 
            p->arrival_time, p->run_time, p->remaining_time, p->waiting_time);

    if (strcmp(action, "finished") == 0) {
        int ta = current_time - p->arrival_time;
        double wta = (double)ta / p->run_time;
        
        wta = round(wta * 100) / 100;

        fprintf(log_file, " TA %d WTA %.2f", ta, wta);
    }

    fprintf(log_file, "\n");
    fflush(log_file); // Flush changes to disk
    
    printf("Logged successfully\n");
}
void initScheduler(int algorithm, int quantum_input, int total_count) {
    // Initialize ready queue
    if (algorithm == 1) { // HPF
        initPriQueue(&ready_queue);
    } else if (algorithm == 2) { // SRTN
        initQueue(&QueueSTRN);
    } else if (algorithm == 3) { // RR
        initQueueQ(&circQ);
        quantum = quantum_input;
    }
    
    // Initialize shared memory for parameters
    int shm_key = ftok("./tmp.txt", 'S');
    if (shm_key == -1) {
        perror("Error generating key for parameters shared memory");
        exit(-1);
    }
    
    shmid = shmget(shm_key, sizeof(struct Parameters), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("Error creating shared memory for parameters");
        exit(-1);
    }
    
    para = (struct Parameters*)shmat(shmid, NULL, 0);
    if (para == (void*)-1) {
        perror("Error attaching shared memory for parameters");
        exit(-1);
    }
    
    pra = (struct Parameters*)malloc(sizeof(struct Parameters));
    if (!pra) {
        perror("Error allocating memory for parameters");
        exit(-1);
    }
    
    // Initialize process shared memory
    int shm_key2 = ftok("./tmp.txt", 'P');
    if (shm_key2 == -1) {
        perror("Error generating key for process shared memory");
        exit(-1);
    }
    
    shmid2 = shmget(shm_key2, sizeof(struct Process), IPC_CREAT | 0666);
    if (shmid2 == -1) {
        perror("Error creating shared memory for process");
        exit(-1);
    }
    
    shared_process = (struct Process*)shmat(shmid2, NULL, 0);
    if (shared_process == (void*)-1) {
        perror("Error attaching shared memory for process");
        exit(-1);
    }
    
    
    // clear the log file to start in an empty file
    FILE* tmp = fopen(LOG_FILE_NAME, "w");
    if (tmp) {
        fprintf(tmp, "#At time x process y state arr w total z remain y wait k\n\n");
        fclose(tmp);
    }
    
    log_file = fopen(LOG_FILE_NAME, "a");
    perf_file = fopen(PERF_FILE_NAME, "w");
    if (!log_file || !perf_file) {
        perror("Failed to open output files");
        exit(EXIT_FAILURE);
    }
    
    // Initialize message queue
    key_t msg_key = ftok("./tmp.txt", 'M');
    if (msg_key == -1) {
        perror("Error in creating key");
        exit(-1);
    }
    
    msg_id = msgget(msg_key, 0666 | IPC_CREAT);
    if (msg_id == -1) {
        perror("Error in creating message queue");
        exit(-1);
    }

    // Set total expected processes
    total_processes = total_count;
    pra->completed_processes = 0;
    pra->cpu_busy_time = 0;
    pra->total_wta = 0;
    pra->total_waiting = 0;
    pra->total_wta_squared = 0;
    memcpy(para, pra, sizeof(struct Parameters));
    scheduler_pid = getpid();

    // Initialize buddy memory manager
    init_memory_manager(&memory_manager);
    // Clear memory log file
    FILE* memlog = fopen("memory.log", "w");
    if (memlog) {
        fprintf(memlog, "#At time x allocated y bytes for process z from i to j\n");
        fclose(memlog);
    }

    initQueue(&waiting_mem_queue);
}

void calculatePerformanceMetrics() {
    FILE* perf = fopen("scheduler.perf", "w");
    if (!perf) {
        perror("Failed to open performance file");
        return;
    }

    // Calculate CPU utilization 
    printf("cpu busy time is %d, at clock %d\n", para->cpu_busy_time, clk);
    double utilization = (double)para->cpu_busy_time / clk * 100;
    utilization = round(utilization * 100) / 100; 

    // Calculate averages 
    double avg_wta = round((para->total_wta / total_processes) * 100) / 100;
    double avg_waiting = round((para->total_waiting / total_processes) * 100) / 100;

    // Calculate standard deviation
    double mean_wta = para->total_wta / total_processes;
    double variance = (para->total_wta_squared / total_processes) - (mean_wta * mean_wta);
    double std_wta = round(sqrt(variance) * 100) / 100;

    fprintf(perf, "CPU utilization = %.2f%%\n", utilization);
    fprintf(perf, "Avg WTA = %.2f\n", avg_wta);
    fprintf(perf, "Avg Waiting = %.2f\n", avg_waiting);
    fprintf(perf, "Std WTA = %.2f\n", std_wta);

    fclose(perf);
}
void RR() {
   
    static struct Process* current_process = NULL;
    static pid_t current_pid = -1;
    static int quantum_start_time = -1;
    
    static int last_debug_print = -1;
    if (clk % 5 == 0 && clk != last_debug_print) {
        printf("===== Time %d: Queue state =====\n", clk);
        printQueueQ(&circQ);
        last_debug_print = clk;
    }
    
    struct Message msg;
    while (msgrcv(msg_id, &msg, sizeof(struct Process), scheduler_pid, IPC_NOWAIT) != -1) {
        struct Process* new_process = (struct Process*)malloc(sizeof(struct Process));
        if (!new_process) {
            perror("Failed to allocate memory for new process");
            continue;
        }
       
        memcpy(new_process, &msg.process, sizeof(struct Process));
        new_process->waiting_time = 0;
        new_process->start_time = -1;
        new_process->last_stop_time = -1;
       
        printf("[RR] Process %d arrived at time %d (memsize=%d)\n", new_process->id, new_process->arrival_time, new_process->memsize);
        
        // Only attempt to allocate memory if the process has actually arrived
        if (new_process->arrival_time <= clk) {
            struct MemoryBlock* block = allocate_memory(&memory_manager, new_process->memsize);
            if (block) {
                new_process->mem_start = block->start;
                new_process->mem_end = block->start + block->size - 1;
                printf("[RR][DEBUG] Allocated process %d at [%d-%d]\n", new_process->id, new_process->mem_start, new_process->mem_end);
                log_memory_action("allocated", clk, new_process->id, new_process->memsize, new_process->mem_start, new_process->mem_end);
                enqueueQ(new_process, &circQ);
            } else {
                printf("[RR][DEBUG] Allocation failed for process %d, putting in waiting queue\n", new_process->id);
                enqueue(new_process, &waiting_mem_queue);
            }
        } else {
            // Process hasn't arrived yet, put it in waiting queue regardless of memory
            printf("[RR][DEBUG] Process %d has future arrival time %d, putting in waiting queue\n", 
                   new_process->id, new_process->arrival_time);
            enqueue(new_process, &waiting_mem_queue);
        }
    }

    try_allocate_waiting_processes(clk, 3);
    printf("[RR][DEBUG] Waiting queue size: %d\n", size(&waiting_mem_queue));
    
    // Check if current process is completed or quantum expired
    if (current_process != NULL) {
        if (shared_process != NULL) {
            memcpy(current_process, shared_process, sizeof(struct Process));
        }
        
        // Check if process has completed
        if (current_process->remaining_time <= 0) {
            printf("Process %d completed at time %d\n", current_process->id, clk);
            // Calculate total time 
            int total_time = clk - current_process->arrival_time;
            current_process->waiting_time = total_time - current_process->run_time;
            logProcessState(clk, current_process->id, "finished", current_process);
            printf("[RR][DEBUG] Freeing memory for process %d at [%d-%d]\n", current_process->id, current_process->mem_start, current_process->mem_end);
            free_memory(&memory_manager, current_process->mem_start, current_process->memsize);
            log_memory_action("freed", clk, current_process->id, current_process->memsize, current_process->mem_start, current_process->mem_end);
            try_allocate_waiting_processes(clk, 3);
            waitpid(current_pid, NULL, WNOHANG);
            free(current_process);
            current_process = NULL;
            current_pid = -1;
            quantum_start_time = -1;
        }
        // Check if quantum has expired
        else if ((clk - quantum_start_time) >= quantum) {
            printf("Quantum expired for process %d at time %d\n", current_process->id, clk);
            
            // Stop the process
            if (kill(current_pid, SIGTSTP) != 0) {
                perror("Failed to stop process");
            }
            
            current_process->last_stop_time = clk;
            
            logProcessState(clk, current_process->id, "stopped", current_process);
            
            // Put the process at the back of the queue for round-robin scheduling
            enqueueQ(current_process, &circQ);
            
            printf("Queue after enqueueing process %d:\n", current_process->id);
            printQueueQ(&circQ);
            
            current_process = NULL;
            current_pid = -1;
            quantum_start_time = -1;
        }
    }
    
    // Schedule a new process if none is running
    if (current_process == NULL && !isEmptyQ(&circQ)) {
        // In Round Robin, we should use FIFO order, not sort by arrival time
        // Take the first process from the queue (which will be the one waiting the longest)
        current_process = dequeueQ(&circQ);
        
        if (current_process->start_time == -1) {
            // first execution
            current_process->start_time = clk;
            // If the process is starting exactly at its arrival time, don't add waiting time
            if (clk == current_process->arrival_time) {
                current_process->waiting_time = 0;
                printf("Process %d started with zero wait (exact arrival)\n", current_process->id);
            } else {
                current_process->waiting_time = clk - current_process->arrival_time;
            }
            logProcessState(clk, current_process->id, "started", current_process);
            printf("Process %d started for first time at %d\n", current_process->id, clk);
        } else {
            // Resuming after preemption
            int wait_time = clk - current_process->last_stop_time;
            current_process->waiting_time += wait_time;
            logProcessState(clk, current_process->id, "resumed", current_process);
            printf("Process %d resumed at %d (waited %d)\n", 
                   current_process->id, clk, wait_time);
        }
        
        // Update shared memory
        if (shared_process != NULL) {
            memcpy(shared_process, current_process, sizeof(struct Process));
        }
        
        // start the actual process
        pid_t pid = fork();
        if (pid == -1) {
            perror("Fork failed");
            enqueueQ(current_process, &circQ);
            current_process = NULL;
            return;
        } else if (pid == 0) {
            char pid_str[16];
            sprintf(pid_str, "%d", scheduler_pid);
            char quantum_str[16];
            sprintf(quantum_str, "%d", quantum);
            execl("./process.out", "process.out", pid_str, quantum_str, NULL);
            perror("Exec failed");
            exit(1);
        } else {
            current_pid = pid;
            quantum_start_time = clk;
            printf("Scheduled process %d at time %d with quantum %d\n", 
                   current_process->id, clk, quantum);
            
            printf("Remaining queue after scheduling process %d:\n", current_process->id);
            printQueueQ(&circQ);
        }
    }
}

char* to_string(int value) {
    static char buffer[32];
    sprintf(buffer, "%d", value);
    return buffer;
}

void quantum_expired_handler(int signum) {
    printf("[RR] Received quantum expiration signal at time %d\n", clk);
    
    if (rr_current_process == NULL) {
        printf("[RR] Warning: Received quantum expiration but no current process\n");
        return;
    }
    
    // stop the current process
    if (rr_current_pid > 0) {
        printf("[RR] Preempting process %d (PID %d) due to quantum expiration\n", 
               rr_current_process->id, rr_current_pid);
        
        // Send stop signal
        if (kill(rr_current_pid, SIGTSTP) == -1) {
            perror("Error stopping process");
            
        }
        
        // update process state
        rr_current_process->last_stop_time = clk;
        
        logProcessState(clk, rr_current_process->id, "stopped", rr_current_process);
        

        enqueueQ(rr_current_process, &circQ);
        
        
        rr_current_process = NULL;
        rr_current_pid = -1;
        rr_needs_scheduling = true;
    }
}

void try_allocate_waiting_processes(int current_time, int algorithm) {
    int allocations_this_pass;
    do {
        allocations_this_pass = 0;
        int n = size(&waiting_mem_queue);
        
        // Create a temporary array to sort processes by arrival time
        struct Process** processes = (struct Process**)malloc(n * sizeof(struct Process*));
        if (!processes) {
            perror("Failed to allocate memory for sorting");
            return;
        }
        
        // Copy processes to array for sorting
        for (int i = 0; i < n; i++) {
            processes[i] = dequeue(&waiting_mem_queue);
        }
        
        // Sort processes by arrival time and process ID
        for (int i = 0; i < n-1; i++) {
            for (int j = 0; j < n-i-1; j++) {
                int should_swap = 0;
                
                // First compare by arrival time
                if (processes[j]->arrival_time > processes[j+1]->arrival_time) {
                    should_swap = 1;
                }
                // If arrival times are equal, compare by process ID
                else if (processes[j]->arrival_time == processes[j+1]->arrival_time) {
                    if (processes[j]->id > processes[j+1]->id) {
                        should_swap = 1;
                    }
                }
                
                if (should_swap) {
                    struct Process* temp = processes[j];
                    processes[j] = processes[j+1];
                    processes[j+1] = temp;
                }
            }
        }
        
        // Print the sorted order for debugging
        printf("[DEBUG] Sorted process order for allocation:\n");
        for (int i = 0; i < n; i++) {
            printf("[DEBUG] Process %d (arrival=%d, id=%d)\n", 
                   processes[i]->id, processes[i]->arrival_time, processes[i]->id);
        }
        
        // Try to allocate memory to processes in order of arrival
        for (int i = 0; i < n; i++) {
            struct Process* p = processes[i];
            
            // Only attempt to allocate memory if the process has actually arrived
            if (p->arrival_time <= current_time) {
                printf("[DEBUG] Attempting allocation for process %d (memsize=%d, arrival=%d)\n", 
                       p->id, p->memsize, p->arrival_time);
                struct MemoryBlock* block = allocate_memory(&memory_manager, p->memsize);
                if (block) {
                    p->mem_start = block->start;
                    p->mem_end = block->start + block->size - 1;
                    printf("[DEBUG] Allocated process %d at [%d-%d]\n", p->id, p->mem_start, p->mem_end);
                    log_memory_action("allocated", current_time, p->id, p->memsize, p->mem_start, p->mem_end);
                    
                    // Special case: If a process arrives exactly at the current time,
                    // ensure it has zero waiting time by adjusting it here
                    if (p->arrival_time == current_time) {
                        printf("[DEBUG] Process %d arrives exactly now - setting to zero wait scheduling\n", p->id);
                        p->waiting_time = 0;
                    }
                    
                    // Enqueue only to the appropriate queue based on algorithm
                    switch (algorithm) {
                        case 1: // HPF
                            enqueuePri(p, &ready_queue);
                            break;
                        case 2: // SRTN
                            enqueue(p, &QueueSTRN);
                            break;
                        case 3: // RR
                            enqueueQ(p, &circQ);
                            break;
                        default:
                            printf("[ERROR] Unknown algorithm: %d\n", algorithm);
                            break;
                    }
                    
                    allocations_this_pass++;
                } else {
                    printf("[DEBUG] Allocation failed for process %d, re-enqueueing\n", p->id);
                    enqueue(p, &waiting_mem_queue);
                }
            } else {
                // Process hasn't arrived yet, put it back in waiting queue
                printf("[DEBUG] Process %d has future arrival time %d, keeping in waiting queue\n", 
                       p->id, p->arrival_time);
                enqueue(p, &waiting_mem_queue);
            }
        }
        
        free(processes);
        
    } while (allocations_this_pass > 0 && size(&waiting_mem_queue) > 0);
    
    printf("[DEBUG] Waiting queue size after allocation: %d\n", size(&waiting_mem_queue));
    printf("[DEBUG] Ready queue size after allocation: %d\n", sizePri(&ready_queue));
}

void HPF() {
    static struct Process* current_process = NULL;
    static pid_t current_pid = -1;
    
    struct Message msg;
    if (msgrcv(msg_id, &msg, sizeof(struct Process), scheduler_pid, IPC_NOWAIT) != -1) {
        struct Process* new_process = (struct Process*)malloc(sizeof(struct Process));
        memcpy(new_process, &msg.process, sizeof(struct Process));
        printf("[DEBUG] New process arrived: id=%d, memsize=%d\n", new_process->id, new_process->memsize);
        
        // Only attempt to allocate memory if the process has actually arrived
        if (new_process->arrival_time <= clk) {
            struct MemoryBlock* block = allocate_memory(&memory_manager, new_process->memsize);
            if (block) {
                new_process->mem_start = block->start;
                new_process->mem_end = block->start + block->size - 1;
                printf("[DEBUG] Allocated process %d at [%d-%d]\n", new_process->id, new_process->mem_start, new_process->mem_end);
                log_memory_action("allocated", clk, new_process->id, new_process->memsize, new_process->mem_start, new_process->mem_end);
                enqueuePri(new_process, &ready_queue);
            } else {
                printf("[DEBUG] Allocation failed for process %d, putting in waiting queue\n", new_process->id);
                enqueue(new_process, &waiting_mem_queue);
            }
        } else {
            // Process hasn't arrived yet, put it in waiting queue regardless of memory
            printf("[DEBUG] Process %d has future arrival time %d, putting in waiting queue\n", 
                   new_process->id, new_process->arrival_time);
            enqueue(new_process, &waiting_mem_queue);
        }
    }
    
    try_allocate_waiting_processes(clk, 1);
    printf("[DEBUG] Ready queue size: %d, Waiting queue size: %d\n", sizePri(&ready_queue), size(&waiting_mem_queue));

    // Display current queue status
    struct pnode* queue_display = ready_queue.front;
    if (queue_display != NULL) {
        printf("Ready queue: ");
        while (queue_display != NULL) {
            printf("P%d(pri=%d, rem=%d, arr=%d) ", 
                   queue_display->process->id, 
                   queue_display->process->priority, 
                   queue_display->process->remaining_time,
                   queue_display->process->arrival_time);
            queue_display = queue_display->next;
        }
        printf("\n");
    }

    // no process running, start one if available
    if (current_process == NULL) {
        if (!isEmptyPri(&ready_queue)) {
            // check if the highest priority process has arrived yet
            struct Process* next_process = peekPri(&ready_queue);
            if (next_process->arrival_time > clk) {
                return;
            }
        
            // Make sure we select the process with earliest arrival time if multiple
            // processes have the same priority
            struct PriQueue temp_queue;
            initPriQueue(&temp_queue);
            
            struct Process* selected_process = NULL;
            
            while (!isEmptyPri(&ready_queue)) {
                struct Process* p = dequeuePri(&ready_queue);
                if (p->arrival_time <= clk) {
                    // Check if this is the first process we've seen with its priority
                    if (selected_process == NULL || p->priority < selected_process->priority) {
                        // If we already had a selected process, move it to temp queue
                        if (selected_process != NULL) {
                            enqueuePri(selected_process, &temp_queue);
                        }
                        selected_process = p;
                    } 
                    // If same priority, select the one that arrived first
                    else if (p->priority == selected_process->priority && 
                             p->arrival_time < selected_process->arrival_time) {
                        enqueuePri(selected_process, &temp_queue);
                        selected_process = p;
                    } 
                    else {
                        enqueuePri(p, &temp_queue);
                    }
                } else {
                    // Process hasn't arrived yet, put it back in the queue
                    enqueuePri(p, &temp_queue);
                }
            }
            
            // Rebuild the original queue with all processes except the selected one
            while (!isEmptyPri(&temp_queue)) {
                struct Process* p = dequeuePri(&temp_queue);
                enqueuePri(p, &ready_queue);
            }
            
            // If we found a process, schedule it
            if (selected_process != NULL) {
                current_process = selected_process;
                
                if (current_process->arrival_time > clk) {
                    printf("Warning: Process %d scheduled to start before its arrival time!\n", current_process->id);
                    current_process->start_time = current_process->arrival_time;
                } else {
                    // Process has arrived
                    current_process->start_time = clk;
                    
                    // Adjust waiting time 
                    if (clk == current_process->arrival_time) {
                        // If process starts exactly at arrival time, don't add waiting time
                        current_process->waiting_time = 0;
                        printf("[HPF] Process %d started with zero wait (exact arrival)\n", current_process->id);
                    } else if (clk > current_process->arrival_time) {
                        current_process->waiting_time = current_process->start_time - current_process->arrival_time;
                    }
                }
                
                strcpy(current_process->state, "running");  
                printf("Starting process %d at time %d, remaining time: %d\n", 
                       current_process->id, clk, current_process->remaining_time);
                
                current_pid = create_process(current_process);
                
                logProcessState(current_process->start_time, current_process->id, "started", current_process);
            }
        }
    }
    // process is running, check if it's finished
    else {
        printf("Checking process %d with PID %d, remaining time: %d\n", 
               current_process->id, current_pid, current_process->remaining_time);
        
        
        int status;
        pid_t wait_result = waitpid(current_pid, &status, WNOHANG);
        
        if (shared_process != NULL) {
            memcpy(current_process, shared_process, sizeof(struct Process));
        }
        
       
        printf("Wait result: %d, Process %d remaining time: %d\n", 
               wait_result, current_process->id, current_process->remaining_time);
        
        if (wait_result > 0 || current_process->remaining_time <= 0) {
            printf("Process %d has finished\n", current_process->id);
            if(current_process->finish_time < 0)
            {
                logProcessState(clk, current_process->id, "finished", current_process);
            }
            else {
                logProcessState(current_process->finish_time, current_process->id, "finished", current_process);
            }
            printf("[RR][DEBUG] Freeing memory for process %d at [%d-%d]\n", current_process->id, current_process->mem_start, current_process->mem_end);
            free_memory(&memory_manager, current_process->mem_start, current_process->memsize);
            log_memory_action("freed", clk, current_process->id, current_process->memsize, current_process->mem_start, current_process->mem_end);
            try_allocate_waiting_processes(clk, 1);
            free(current_process);
            current_process = NULL;
            current_pid = -1;
            
            if (!isEmptyPri(&ready_queue)) {
                struct Process* next_process = peekPri(&ready_queue);
                if (next_process->arrival_time <= clk) {
                    current_process = dequeuePri(&ready_queue);
                    current_process->start_time = clk;
                    
                    // calculate waiting time
                    if (clk > current_process->arrival_time) {
                        current_process->waiting_time = clk - current_process->arrival_time;
                    }
                    
                    strcpy(current_process->state, "running");
                    
                    current_pid = create_process(current_process);
                    
                    logProcessState(clk, current_process->id, "started", current_process);
                    
                    printf("Immediately starting next process %d at time %d\n", 
                           current_process->id, clk);
                }
            }
        }
    }
    
    
    struct pnode* temp = ready_queue.front;
    while (temp != NULL) {
        if (strcmp(temp->process->state, "running") != 0 && 
            temp->process->arrival_time <= clk) {
            strcpy(temp->process->state, "ready");
            temp->process->waiting_time++;
        }
        temp = temp->next;
    }
}

void SRTN() {
    printf("[DEBUG] SRTN function entered at clk=%d\n", clk);
    static struct Process* running_process = NULL;  
    static pid_t running_pid = -1;                  
    static int exec_start_time = 0;                 
    
    struct Message msg;
    while (1) {
        printf("[DEBUG] SRTN waiting for message at clk=%d\n", clk);
        int got_msg = msgrcv(msg_id, &msg, sizeof(struct Process), scheduler_pid, IPC_NOWAIT);
        printf("[DEBUG] SRTN got message? %d\n", got_msg);
        if (got_msg == -1) break;
        struct Process* new_process = (struct Process*)malloc(sizeof(struct Process));
        memcpy(new_process, &msg.process, sizeof(struct Process));
        new_process->waiting_time = 0;
        new_process->start_time = -1;
        new_process->last_stop_time = 0;
        new_process->preemption_count = 0;
        strcpy(new_process->state, STATE_ARRIVED);
        printf("[SRTN] Time %d: Process %d arrived (runtime=%d, memsize=%d)\n",
                new_process->arrival_time, new_process->id, new_process->run_time, new_process->memsize);
        
        // Only attempt to allocate memory if the process has actually arrived
        if (new_process->arrival_time <= clk) {
            struct MemoryBlock* block = allocate_memory(&memory_manager, new_process->memsize);
            if (block) {
                new_process->mem_start = block->start;
                new_process->mem_end = block->start + block->size - 1;
                printf("[SRTN][DEBUG] Allocated process %d at [%d-%d]\n", new_process->id, new_process->mem_start, new_process->mem_end);
                log_memory_action("allocated", clk, new_process->id, new_process->memsize, new_process->mem_start, new_process->mem_end);
                enqueue(new_process, &QueueSTRN);
            } else {
                printf("[SRTN][DEBUG] Allocation failed for process %d, putting in waiting queue\n", new_process->id);
                enqueue(new_process, &waiting_mem_queue);
            }
        } else {
            // Process hasn't arrived yet, put it in waiting queue regardless of memory
            printf("[SRTN][DEBUG] Process %d has future arrival time %d, putting in waiting queue\n", 
                   new_process->id, new_process->arrival_time);
            enqueue(new_process, &waiting_mem_queue);
        }
    }
    try_allocate_waiting_processes(clk, 2);
    printf("[SRTN][DEBUG] Ready queue size: %d, Waiting queue size: %d\n", size(&QueueSTRN), size(&waiting_mem_queue));

    if (running_process != NULL && shared_process != NULL) {
        
        int prev_remaining = running_process->remaining_time;
        memcpy(running_process, shared_process, sizeof(struct Process));
        
        
        strcpy(running_process->state, STATE_RUNNING);
        
        // If process has finished its execution
        if (running_process->remaining_time <= 0) {
            printf("[SRTN] Time %d: Process %d completed execution\n", clk, running_process->id);
            running_process->finish_time = clk;
            int total_time = running_process->finish_time - running_process->arrival_time;
            running_process->waiting_time = total_time - running_process->run_time;
            para->total_waiting += running_process->waiting_time;
            logProcessState(clk, running_process->id, "finished", running_process);
            printf("[SRTN][DEBUG] Freeing memory for process %d at [%d-%d]\n", running_process->id, running_process->mem_start, running_process->mem_end);
            free_memory(&memory_manager, running_process->mem_start, running_process->memsize);
            log_memory_action("freed", clk, running_process->id, running_process->memsize, running_process->mem_start, running_process->mem_end);
            try_allocate_waiting_processes(clk, 2);
            waitpid(running_pid, NULL, WNOHANG);
            free(running_process);
            running_process = NULL;
            running_pid = -1;
        }
        else {
            // Process is still running, update execution time tracking
            printf("[SRTN] Time %d: Process %d running (remaining: %d->%d)\n", 
                   clk, running_process->id, prev_remaining, running_process->remaining_time);
        }
    }
    

    // Find the process with shortest remaining time that has actually arrived
    struct Process* shortest = NULL;
    struct node* temp = QueueSTRN.front;
    struct node* shortest_node = NULL;
    struct node* prev = NULL;
    struct node* shortest_prev = NULL;
    
    // First pass: find the earliest arrived process
    struct Process* earliest = NULL;
    struct node* earliest_node = NULL;
    struct node* earliest_prev = NULL;
    
    while (temp != NULL) {
        // Only consider processes that have already arrived
        if (temp->process->arrival_time <= clk) {
            if (earliest == NULL || temp->process->arrival_time < earliest->arrival_time) {
                earliest = temp->process;
                earliest_node = temp;
                earliest_prev = prev;
            }
            
            if (shortest == NULL || temp->process->remaining_time < shortest->remaining_time) {
                shortest = temp->process;
                shortest_node = temp;
                shortest_prev = prev;
            } else if (temp->process->remaining_time == shortest->remaining_time && 
                       temp->process->arrival_time < shortest->arrival_time) {
                // If two processes have the same remaining time, select the one that arrived first
                shortest = temp->process;
                shortest_node = temp;
                shortest_prev = prev;
            }
        }
        prev = temp;
        temp = temp->next;
    }
    
    // If we found both an earliest and a shortest process, but they're different,
    // Check if earliest arrived in this exact clock tick - if so, prioritize it
    if (earliest != NULL && shortest != NULL && earliest != shortest && earliest->arrival_time == clk) {
        printf("[SRTN] Prioritizing newly arrived process %d over shortest process %d\n", 
                earliest->id, shortest->id);
        shortest = earliest;
        shortest_node = earliest_node;
        shortest_prev = earliest_prev;
    }
    
    if (running_process == NULL && shortest != NULL) {
        // Remove shortest process from queue
        if (shortest_prev == NULL) {
            QueueSTRN.front = shortest_node->next;
        } else {
            shortest_prev->next = shortest_node->next;
        }
        free(shortest_node);
        
        running_process = shortest;
        exec_start_time = clk;
        
        // First run or resuming after preemption
        if (running_process->start_time == -1) {
            running_process->start_time = clk;
            // If process starts exactly at arrival time, don't add waiting time
            if (clk == running_process->arrival_time) {
                running_process->waiting_time = 0;
                printf("[SRTN] Process %d started with zero wait (exact arrival)\n", running_process->id);
            } else {
                // Calculate initial waiting time
                running_process->waiting_time = clk - running_process->arrival_time;
            }
            strcpy(running_process->state, STATE_STARTED);
            
            running_pid = create_process(running_process);
            logProcessState(clk, running_process->id, "started", running_process);
            printf("[SRTN] Time %d: Started process %d (first time)\n", 
                   clk, running_process->id);
        } else {
            // Resuming after preemption
            // Calculate waiting time since last stop
            int wait_time = clk - running_process->last_stop_time;
            running_process->waiting_time += wait_time;
            strcpy(running_process->state, STATE_RESUMED);
            
            running_pid = create_process(running_process);
            logProcessState(clk, running_process->id, "resumed", running_process);
            printf("[SRTN] Time %d: Resumed process %d (preempted %d times)\n", 
                   clk, running_process->id, running_process->preemption_count);
        }
    }
    //  preemption
    else if (running_process != NULL && shortest != NULL && 
             shortest->remaining_time < running_process->remaining_time) {
        
        printf("[SRTN] Time %d: Preempting process %d (rem=%d) with process %d (rem=%d)\n",
               clk, running_process->id, running_process->remaining_time,
               shortest->id, shortest->remaining_time);
        
        // Stop the current process
        stop_process(running_pid);
        
        running_process->last_stop_time = clk;
        running_process->preemption_count++;
        strcpy(running_process->state, STATE_STOPPED);
        
        // Calculate waiting time after preemption
        int preemption_wait = clk - running_process->last_stop_time;
        running_process->waiting_time += preemption_wait;
        
        logProcessState(clk, running_process->id, "stopped", running_process);
        
        // return preempted process to queue
        enqueue(running_process, &QueueSTRN);
        
        // remove shortest process mn queue
        if (shortest_prev == NULL) {
            QueueSTRN.front = shortest_node->next;
        } else {
            shortest_prev->next = shortest_node->next;
        }
        free(shortest_node);
        
        // Make shortest the new running process
        running_process = shortest;
        exec_start_time = clk;
        
        // awl mara y run or resume b3d preemption
        if (running_process->start_time == -1) {
            // First time starting
            running_process->start_time = clk;
            strcpy(running_process->state, STATE_STARTED);
            
            // start process
            running_pid = create_process(running_process);
            logProcessState(clk, running_process->id, "started", running_process);
            printf("[SRTN] Time %d: Started process %d (first time)\n", 
                   clk, running_process->id);
        } else {
            // resuming b3d preemption
            strcpy(running_process->state, STATE_RESUMED);
            
            // start  process
            running_pid = create_process(running_process);
            logProcessState(clk, running_process->id, "resumed", running_process);
            printf("[SRTN] Time %d: Resumed process %d (preempted %d times)\n", 
                   clk, running_process->id, running_process->preemption_count);
        }
    }
    // Process running, no preemption needed
    else if (running_process != NULL) {
        // Continue running current process
    }
    // No process running, none available
    else {
        // Nothing to do
    }
}

void cleanup() {
    // Clean up message queue
    msgctl(msg_id, IPC_RMID, NULL);
    


    clear(&QueueSTRN);
    destroyPri(&ready_queue);
    
    
    if (log_file != NULL) {
        fclose(log_file);
    }
    
    if (perf_file != NULL) {
        fclose(perf_file);
    }
     shmdt(para);
    shmdt(shared_process);
    shmctl(shmid, IPC_RMID, NULL);
    shmctl(shmid2, IPC_RMID, NULL);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Error: insufficient arguments\n");
        printf("Usage: %s <algorithm_number> <quantum> <process_count>\n", argv[0]);
        exit(1);
    }

    
    int algorithm = atoi(argv[1]);
    int quantum_input = atoi(argv[2]);
    int process_count = atoi(argv[3]);
    
    printf("Scheduler starting with algorithm %d, quantum %d, process count %d\n", 
           algorithm, quantum_input, process_count);
    
    // scheduler PID for message queue
    scheduler_pid = getpid();
    printf("Scheduler PID: %d\n", scheduler_pid);
    
    
    signal(SIGUSR1, process_finished_handler);
    signal(SIGUSR2, quantum_expired_handler); 
    
    // Initialize clock
    initClk();
    
    // Initialize scheduler
    initScheduler(algorithm, quantum_input, process_count);

    
    last_clk = -1;
    while (para->completed_processes < process_count) {
        // Get current time
        clk = getClk();
        
        // Process any incoming messages immediately, regardless of clock changes
        // This ensures processes are handled as soon as they arrive
        int received_new_processes = false;
        struct Message msg;
        switch(algorithm) {
            case 1:
                // Check for new messages - HPF will process them
                if (msgrcv(msg_id, &msg, sizeof(struct Process), scheduler_pid, IPC_NOWAIT) != -1) {
                    // Put the message back so HPF can process it normally
                    msgsnd(msg_id, &msg, sizeof(struct Process), 0);
                    received_new_processes = true;
                }
                break;
            case 2:
                // Check for new messages - SRTN will process them
                if (msgrcv(msg_id, &msg, sizeof(struct Process), scheduler_pid, IPC_NOWAIT) != -1) {
                    // Put the message back so SRTN can process it normally
                    msgsnd(msg_id, &msg, sizeof(struct Process), 0);
                    received_new_processes = true;
                }
                break;
            case 3:
                // Check for new messages - RR will process them
                if (msgrcv(msg_id, &msg, sizeof(struct Process), scheduler_pid, IPC_NOWAIT) != -1) {
                    // Put the message back so RR can process it normally
                    msgsnd(msg_id, &msg, sizeof(struct Process), 0);
                    received_new_processes = true;
                }
                break;
        }
        
        // Only run algorithm when clock changes or we received new processes
        if (clk > last_clk || received_new_processes) {
            printf("Clock: %d, Completed: %d/%d\n", clk, para->completed_processes, process_count);
            
            // Run scheduler algorithm multiple times in the same tick to ensure all
            // newly arrived processes are immediately scheduled if possible
            bool changes_made;
            int num_iterations = 0;
            do {
                // Track if any changes were made to process states
                changes_made = false;
                
                // Save the number of processes in each queue before running the algorithm
                int pri_queue_size_before = sizePri(&ready_queue);
                int srtn_queue_size_before = size(&QueueSTRN);
                int rr_queue_size_before = sizeQ(&circQ);
                
                // Run the selected algorithm
                switch(algorithm) {
                    case 1:
                        HPF();
                        changes_made = pri_queue_size_before != sizePri(&ready_queue);
                        break;
                    case 2:
                        SRTN();
                        changes_made = srtn_queue_size_before != size(&QueueSTRN);
                        break;
                    case 3:
                        RR();
                        changes_made = rr_queue_size_before != sizeQ(&circQ);
                        break;
                    default:
                        printf("Error: Invalid algorithm number\n");
                        exit(1);
                }
                
                // After each algorithm run, try to allocate waiting processes
                // This ensures we don't wait for the next clock tick to allocate memory
                if (size(&waiting_mem_queue) > 0) {
                    printf("[DEBUG] Checking waiting processes after algorithm run\n");
                    try_allocate_waiting_processes(clk, algorithm);
                }
                
                // Limit the number of iterations to prevent infinite loops
                num_iterations++;
            } while (changes_made && num_iterations < 5); // 5 iterations should be enough
            
            last_clk = clk;
        }
    }
    
    printf("All processes completed!\n");
    
    // performance metrics
    clk = getClk();
    memcpy(pra, para, sizeof(struct Parameters));
    calculatePerformanceMetrics();
    

    cleanup();
    destroyClk(false);
    return 0;
}