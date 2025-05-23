# OS Scheduler

A comprehensive Operating System Scheduler implementation that includes process scheduling, memory management, and inter-process communication.

## Overview

This project implements a complete operating system scheduler with the following key components:

- Process Scheduler
- Memory Manager
- Process Generator
- Clock Module
- Inter-Process Communication

## Components

### 1. Process Scheduler (`scheduler.c`)
- Implements various scheduling algorithms
- Manages process states and transitions
- Handles process execution and context switching

### 2. Memory Manager (`memory.c`, `memory.h`)
- Implements a buddy system for memory allocation
- Handles dynamic memory allocation and deallocation
- Manages memory fragmentation

### 3. Process Generator (`process_generator.c`)
- Generates test processes with different characteristics
- Simulates real-world process creation patterns
- Helps in testing the scheduler's performance

### 4. Clock Module (`clk.c`)
- Provides system time functionality
- Manages timing for process scheduling
- Ensures accurate time-based operations

### 5. Process Module (`process.c`)
- Implements process control and management
- Handles process creation and termination
- Manages process resources

## Building the Project

To build the project, use the provided Makefile:

```bash
make
```

This will compile all components and create the necessary executables.

## Running the Project



Start the process generator:
```bash
./process_generator.out
```



## Project Structure

```
OS-Scheduler/
├── clk.c                 # Clock module implementation
├── memory.c             # Memory manager implementation
├── memory.h             # Memory manager header
├── process.c            # Process module implementation
├── process_generator.c  # Process generator implementation
├── scheduler.c          # Scheduler implementation
├── headers.h            # Common header definitions
├── Message.h            # IPC message definitions
├── ProcessControl.h     # Process control structures
└── Data Structures/     # Custom data structures
    ├── CircularQ.h      # Circular queue implementation
    ├── PriQueue.h       # Priority queue implementation
    ├── Process.h        # Process structure definitions
    └── Queue.h          # Queue implementation
```

## Features

- Efficient process scheduling
- Dynamic memory management using buddy system
- Inter-process communication
- Process state management
- Resource allocation and deallocation
- Time-based scheduling
- Priority-based process execution


## License

This project is public but read-only. Users can view and learn from the code but cannot modify it.

## Author

Mohamed Karamallah 
