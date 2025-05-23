//processgen
#include "headers.h"
#include "Data Structures/Queue.h"

void clearResources(int);
struct Message msg;
int msg_id;
int msg_key;
struct Queue* queue;
char choice_str[10];
char quantum_str[10];
char queue_size_str[10];

int main(int argc, char * argv[])
{
    signal(SIGINT, clearResources);
    
    // Initialize queue
    queue = (struct Queue*)malloc(sizeof(struct Queue));
    initQueue(queue);

    // read processes from file
    FILE *file = fopen("processes.txt", "r");
    if (file == NULL) {
        printf("Error opening file\n");
        return 1;
    } //a deddooooo
   
    char buffer[256];
    while(fgets(buffer, sizeof(buffer), file) != NULL) {
        if(buffer[0] == '#') continue;
        
        int id, arrival_time, run_time, priority, memsize;
        if(sscanf(buffer, "%d %d %d %d %d", &id, &arrival_time, &run_time, &priority, &memsize) == 5) {
            struct Process* process = (struct Process*)malloc(sizeof(struct Process));
            process->id = id;
            process->arrival_time = arrival_time;
            process->run_time = run_time;
            process->priority = priority;
            process->remaining_time = run_time;
            process->waiting_time = 0;
            process->start_time = -1;
            process->finish_time = -1;
            process->last_stop_time = -1;
            process->preemption_count = 0;
            strcpy(process->state, "ready");
            process->memsize = memsize;
            process->mem_start = -1;
            process->mem_end = -1;
            enqueue(process, queue);
        }
    }
    fclose(file);

    // get scheduling algorithm choice
    printf("Choose a desired scheduling algorithm:\n");
    printf("1. Non-preemptive Highest Priority First (HPF)\n");
    printf("2. Shortest Remaining Time Next (SRTN)\n");
    printf("3. Round Robin (RR)\n");
    printf("Enter your choice from (1-3): ");
    
    int choice;
    scanf("%d", &choice);
    
    int Q = 0; //eh Q deh ya3am esmaha quantum
    // tab ma heya Q 3ady to indicate quantum
    if(choice == 3) {
        printf("Enter the time quantum: ");
        scanf("%d", &Q);
    }
    
    sprintf(choice_str, "%d", choice);
    sprintf(quantum_str, "%d", Q);
    sprintf(queue_size_str, "%d", size(queue));

    // message queue to communicate with the scheduler  
    msg_key = ftok("./tmp.txt", 'M');
    if (msg_key == -1) {
        perror("Error in creating key");
        exit(-1);
    }
    
    msg_id = msgget(msg_key, 0666 | IPC_CREAT);
    if (msg_id == -1) {
        perror("Error in creating message queue");
        exit(-1);
    }

    // forking to create child process that will run the scheduler
    int scheduler_pid = fork();
    if(scheduler_pid == -1) {
        perror("Error in creating scheduler process");
        exit(-1);
    }
    
    if(scheduler_pid == 0) {
        if(execl("./scheduler.out", "scheduler.out", choice_str, quantum_str, queue_size_str, NULL) == -1) {
            perror("Error in executing scheduler.out");
            exit(-1);
        }
    }

    // create clock process
    int clock_pid = fork();
    if(clock_pid == -1) {
        perror("Error in creating clock process");
        exit(-1);
    }
    
    if(clock_pid == 0) {
        if(execl("./clk.out", "clk.out", NULL) == -1) {
            perror("Error in executing clk.out");
            exit(-1);
        }
    }

    // initialize clock
    sleep(1);
    initClk();

    // send processes to scheduler
    printf("Queue size: %d\n", size(queue));
    struct Process* sneak = peek(queue);
    while(!isEmpty(queue)) {
        int current_time = getClk();
       
        // Send processes 1 tick BEFORE their actual arrival time
        // to compensate for scheduler's message processing delay
        if(sneak && current_time >= (sneak->arrival_time-1)) {
            struct Process* process = dequeue(queue);
            printf("Sending process %d at time %d for arrival at time %d\n", 
                   process->id, current_time, process->arrival_time);
            
            // copy the process to the message
        
            msg.mtype = scheduler_pid;
            memcpy(&msg.process, process, sizeof(struct Process));
            
            if(msgsnd(msg_id, &msg, sizeof(struct Process), 0) == -1) {
                perror("Error in sending message");
                free(process);
                exit(-1);
            }
            
            free(process); 
            sneak = peek(queue);
        }
    }

    // wait for scheduler to finish so it doesn't die before the clock
    int status;
    waitpid(scheduler_pid, &status, 0);

    // Cleanup
    destroyClk(true);
    msgctl(msg_id, IPC_RMID, NULL);
    destroy(queue);
    return 0;
}

void clearResources(int signum)
{
    printf("\nCleaning up resources...\n");
    destroyClk(true);
    msgctl(msg_id, IPC_RMID, NULL);
    destroy(queue);
    exit(0);
}