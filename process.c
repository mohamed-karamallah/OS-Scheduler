//prcs.c
//prs.c
#include "headers.h"
#include "Data Structures/Process.h"



int scheduler_pid;
int running = 1; // Flag to indicate if process is running or stopped
int quantum = -1; 
int quantum_remaining = -1; 

// Signal handlers
void handler_stop(int signum);
void handler_cont(int signum);
void handler_term(int signum);
struct Parameters {
    int completed_processes;
    int cpu_busy_time;
    double total_wta;
    double total_waiting;
    double total_wta_squared;
};
struct Process* shared_process;

int main(int argc, char * argv[])
{
    initClk();
    
   
    if (argc < 2) {
        printf("Error: insufficient arguments for process\n");
        exit(1);
    }
    
    
    scheduler_pid = atoi(argv[1]);
    
  
    if (argc >= 3) {
        quantum = atoi(argv[2]);
        quantum_remaining = quantum;
        printf("Process started with scheduler PID %d, quantum %d\n", 
               scheduler_pid, quantum);
    } else {
        printf("Process started with scheduler PID %d (no quantum)\n", scheduler_pid);
    }
    
    // shared memory 
    int shm_key = ftok("./tmp.txt", 'P');
    int shmid = shmget(shm_key, sizeof(struct Process), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("Failed to get shared memory for process");
        exit(1);
    }
    
    shared_process = (struct Process*)shmat(shmid, NULL, 0);
   
    
    // shared memory for performance parameters
    int shm_key2 = ftok("./tmp.txt", 'S');
    int shmid2 = shmget(shm_key2, sizeof(struct Parameters), IPC_CREAT | 0666);
    if (shmid2 == -1) {
        perror("Failed to get shared memory for parameters");
        exit(1);
    }
    
    struct Parameters* pra = (struct Parameters*)shmat(shmid2, NULL, 0);
 
    
   
    signal(SIGTSTP, handler_stop); 
    signal(SIGCONT, handler_cont); 
    signal(SIGTERM, handler_term); 
    
    printf("Process %d: Starting execution with remaining time %d\n", 
           shared_process->id, shared_process->remaining_time);
    
    // Wait until the actual arrival time if not yet reached
    int current_time = getClk();
    if (current_time < shared_process->arrival_time) {
        printf("Process %d: Waiting until arrival time %d (current: %d)\n", 
               shared_process->id, shared_process->arrival_time, current_time);
        while(getClk() < shared_process->arrival_time && running);
        printf("Process %d: Reached arrival time %d\n", 
               shared_process->id, shared_process->arrival_time);
    }
    
    // Main execution loop
    int last_clk = getClk();
    
    while (shared_process->remaining_time > 0 && running)
    {
        // Wait for a clock 
        current_time = getClk();
        if (current_time == last_clk) {
            continue; // Still in the same clock 
        }
        
       
        last_clk = current_time;
        
        // CPU statistics and remaining time
        pra->cpu_busy_time++;
        shared_process->remaining_time--;
        
        printf("Process %d: At clock %d, remaining time is %d\n", 
               shared_process->id, current_time, shared_process->remaining_time);
        
        // For Round Robin
        if (quantum > 0) {
            quantum_remaining--;
            printf("Process %d: Quantum remaining: %d\n", 
                   shared_process->id, quantum_remaining);
            
            if (quantum_remaining == 0) {
                printf("Process %d: Quantum expired at time %d, notifying scheduler\n", 
                       shared_process->id, current_time);
                kill(scheduler_pid, SIGUSR2);
                
                
                quantum_remaining = quantum;
                
               
            }
        }
        
        // Check if process is complete
        if (shared_process->remaining_time <= 0) {
            printf("Process %d: Execution complete at time %d\n", 
                   shared_process->id, current_time);
            
            
            shared_process->finish_time = current_time;
            
            // Calculate turnaround time and WTA
            int ta = shared_process->finish_time - shared_process->arrival_time;
            double wta = (double)ta / shared_process->run_time;
            
            printf("Process %d finished: TA=%d, WTA=%.2f\n", 
                   shared_process->id, ta, wta);
            
            // Update performance metrics
            pra->total_wta += wta;
            pra->total_waiting += shared_process->waiting_time;
            printf("Process %d: Total waiting time: %f\n", shared_process->id, pra->total_waiting);
            pra->total_wta_squared += wta * wta;
            pra->completed_processes++;
            
            // Notify scheduler that process is complete
            kill(scheduler_pid, SIGUSR1);
            break;
        }
    }
    
    // Clean up
    shmdt(pra);
    shmdt(shared_process);
    destroyClk(false);
    
    return 0;
}


void handler_stop(int signum)
{
    running = 0;
    printf("Process %d: Stopped with remaining time %d\n", 
           shared_process->id, shared_process->remaining_time);
    
    // Block until resumed
    pause();
}


void handler_cont(int signum)
{
    running = 1;
    printf("Process %d: Resumed with remaining time %d, quantum reset to %d\n", 
           shared_process->id, shared_process->remaining_time, quantum);
    
    // Reset quantum counter when process is resumed
    quantum_remaining = quantum;
}


void handler_term(int signum)
{
    printf("Process %d: Terminated with remaining time %d\n", 
           shared_process->id, shared_process->remaining_time);
    destroyClk(false);
    exit(0);
}