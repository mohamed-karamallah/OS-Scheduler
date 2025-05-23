//memoryH
#ifndef MEMORY_H
#define MEMORY_H

#include <stdbool.h>

#define TOTAL_MEMORY_SIZE 1024

// Structure for a memory block in the buddy system
struct MemoryBlock {
    int start;                // Start index in memory
    int size;                 // Size of the block
    bool is_free;             // Is this block free?
    struct MemoryBlock* left; // Left buddy (if split)
    struct MemoryBlock* right;// Right buddy (if split)
    struct MemoryBlock* parent; // Parent block
};

// Memory manager holding the root block
struct MemoryManager {
    struct MemoryBlock* root;
};

// API
void init_memory_manager(struct MemoryManager* manager);
struct MemoryBlock* allocate_memory(struct MemoryManager* manager, int size);
void free_memory(struct MemoryManager* manager, int start, int size);
void log_memory_action(const char* action, int time, int pid, int size, int start, int end);
void destroy_memory_manager(struct MemoryManager* manager);

#endif // MEMORY_H